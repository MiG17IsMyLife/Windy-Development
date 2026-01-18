#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <elf.h>

#pragma pack(push, 1)

int main(int argc, char* argv[]) {
    std::cout << "--- Lindbergh ELF Loader 'Windy' (Phase 2: Symbol Mapping) ---" << std::endl;

    if (argc < 2) {
        std::cout << "使用法: Windy.exe <id5.elfのパス>" << std::endl;
        return 1;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "エラー: ファイルが開けません。" << std::endl;
        return 1;
    }

    // 1. ELFヘッダーの読み取り
    Elf32_Ehdr ehdr;
    file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        std::cerr << "エラー: 有効なELFファイルではありません。" << std::endl;
        return 1;
    }

    // 2. メモリ範囲の計算
    uintptr_t minAddr = 0xFFFFFFFF, maxAddr = 0;
    for (int i = 0; i < ehdr.e_phnum; ++i) {
        file.seekg(ehdr.e_phoff + (i * sizeof(Elf32_Phdr)));
        Elf32_Phdr phdr;
        file.read(reinterpret_cast<char*>(&phdr), sizeof(phdr));
        if (phdr.p_type == PT_LOAD) {
            if (phdr.p_vaddr < minAddr) minAddr = phdr.p_vaddr;
            if (phdr.p_vaddr + phdr.p_memsz > maxAddr) maxAddr = phdr.p_vaddr + phdr.p_memsz;
        }
    }

    uintptr_t reservedBase = minAddr & ~0xFFFF;
    uintptr_t reservedSize = (maxAddr - reservedBase + 0xFFFF) & ~0xFFFF;

    // 3. メモリの一括予約
    LPVOID totalArea = VirtualAlloc((LPVOID)reservedBase, reservedSize, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!totalArea) {
        std::cerr << "メモリ予約失敗。0x20000000設定を確認してください。" << std::endl;
        return 1;
    }

    // 4. セグメントの展開
    for (int i = 0; i < ehdr.e_phnum; ++i) {
        file.seekg(ehdr.e_phoff + (i * sizeof(Elf32_Phdr)));
        Elf32_Phdr phdr;
        file.read(reinterpret_cast<char*>(&phdr), sizeof(phdr));
        if (phdr.p_type == PT_LOAD) {
            uintptr_t alignedVAddr = phdr.p_vaddr & ~0xFFF;
            uintptr_t offsetInPage = phdr.p_vaddr & 0xFFF;
            uintptr_t alignedSize = (phdr.p_memsz + offsetInPage + 0xFFF) & ~0xFFF;
            VirtualAlloc((LPVOID)alignedVAddr, alignedSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (phdr.p_filesz > 0) {
                file.seekg(phdr.p_offset);
                file.read((char*)phdr.p_vaddr, phdr.p_filesz);
            }
            if (phdr.p_memsz > phdr.p_filesz) {
                memset((char*)phdr.p_vaddr + phdr.p_filesz, 0, phdr.p_memsz - phdr.p_filesz);
            }
        }
    }
    std::cout << "[OK] メモリ展開完了。" << std::endl;

    // 5. 【重要】動的セクションとシンボルの解析
    std::cout << "\n--- インポート関数の一覧 ---" << std::endl;
    uintptr_t symTab = 0, strTab = 0, jmpRel = 0;
    uint32_t pltRelSize = 0;

    for (int i = 0; i < ehdr.e_phnum; ++i) {
        file.seekg(ehdr.e_phoff + (i * sizeof(Elf32_Phdr)));
        Elf32_Phdr phdr;
        file.read((char*)&phdr, sizeof(phdr));
        if (phdr.p_type == PT_DYNAMIC) {
            size_t dynCount = phdr.p_filesz / sizeof(Elf32_Dyn);
            std::vector<Elf32_Dyn> dyns(dynCount);
            file.seekg(phdr.p_offset);
            file.read((char*)dyns.data(), phdr.p_filesz);

            for (const auto& d : dyns) {
                switch (d.d_tag) {
                case DT_SYMTAB: symTab = d.d_un.d_ptr; break;
                case DT_STRTAB: strTab = d.d_un.d_ptr; break;
                case DT_JMPREL: jmpRel = d.d_un.d_ptr; break;
                case DT_PLTRELSZ: pltRelSize = d.d_un.d_val; break;
                }
            }
        }
    }

    if (jmpRel && symTab && strTab) {
        size_t relCount = pltRelSize / sizeof(Elf32_Rel);
        for (size_t i = 0; i < relCount; ++i) {
            Elf32_Rel* rel = (Elf32_Rel*)(jmpRel + i * sizeof(Elf32_Rel));
            uint32_t symIdx = ELF32_R_SYM(rel->r_info);
            Elf32_Sym* sym = (Elf32_Sym*)(symTab + symIdx * sizeof(Elf32_Sym));
            const char* name = (const char*)(strTab + sym->st_name);

            // 0x0で落ちていた原因の「住所録（GOT）」のアドレスを表示
            std::cout << "  [GOT: 0x" << std::hex << rel->r_offset << "] " << name << std::endl;
        }
    }

    // 6. Linuxスタック構築
    uint32_t stackSize = 1024 * 1024;
    void* linuxStack = VirtualAlloc(NULL, stackSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint32_t esp = (uint32_t)linuxStack + stackSize - 64;
    uint32_t* stackPtr = (uint32_t*)esp;
    const char* dummyArgv0 = "id5.elf";
    *(--stackPtr) = 0; *(--stackPtr) = 0; *(--stackPtr) = (uint32_t)dummyArgv0; *(--stackPtr) = 1;

    uint32_t finalEsp = (uint32_t)stackPtr, entryPoint = ehdr.e_entry;
    std::cout << "\nジャンプ準備完了。3秒後に実行開始..." << std::endl;
    Sleep(3000);

    // 7. 運命のジャンプ
    __asm {
        mov esp, finalEsp
        xor eax, eax
        xor ebx, ebx
        xor ecx, ecx
        xor edx, edx
        jmp entryPoint
    }
    return 0;
}
#pragma pack(pop)