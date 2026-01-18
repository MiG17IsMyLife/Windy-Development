#define _CRT_SECURE_NO_WARNINGS
#include "ProcessBridge.h"
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <process.h>
#include <stdarg.h>
#include <errno.h>

extern "C" {

    // ----------------------------------------------------------------
    // fork: Create a child process
    // ----------------------------------------------------------------
    // Windowsではfork()による完全なメモリ空間の複製は不可能なため、
    // 常に「親プロセス」として振る舞う(正のPIDを返す)ことで、
    // ゲーム側の「子プロセス用ブロック」をスキップさせます。
    // ----------------------------------------------------------------
    int my_fork(void) {
        std::cout << "[Process] fork called. Pretending to be Parent (PID=1000)." << std::endl;
        return 1000;
    }

    // ----------------------------------------------------------------
    // vfork: Create a child process and block parent
    // ----------------------------------------------------------------
    // forkと同じ扱いとします。
    // ----------------------------------------------------------------
    int my_vfork(void) {
        return my_fork();
    }

    // ----------------------------------------------------------------
    // daemon: Run in background
    // ----------------------------------------------------------------
    // エミュレータではコンソールを表示したままにしたいことが多いため、
    // 何もせず成功(0)を返します。
    // ----------------------------------------------------------------
    int my_daemon(int nochdir, int noclose) {
        std::cout << "[Process] daemon called. Success." << std::endl;
        return 0;
    }

    // ----------------------------------------------------------------
    // execlp: Execute a file
    // ----------------------------------------------------------------
    // 現在のプロセスを置換するのではなく、_spawnvpを使って
    // 新しいプロセスとして外部コマンドを実行します。
    // ----------------------------------------------------------------
    int my_execlp(const char* file, const char* arg, ...) {
        std::cout << "[Process] execlp called: " << file << std::endl;

        // 可変長引数の解析
        std::vector<const char*> args;
        args.push_back(arg);

        va_list ap;
        va_start(ap, arg);
        while (1) {
            const char* p = va_arg(ap, const char*);
            if (p == NULL) break;
            args.push_back(p);
        }
        va_end(ap);
        args.push_back(NULL); // 終端NULL

        // パス変換ロジック
        // Linuxのシェルスクリプト実行要求を、Windowsのsh.exe等にマッピングします
        std::string prog = file;
        if (prog == "sh" || prog == "/bin/sh") {
            // 環境に合わせてパスを変更してください
            // MSYS2のパスが通っている場合は単に "sh" でも動く可能性があります
            prog = "C:\\msys64\\usr\\bin\\sh.exe";
        }

        // 非同期実行 (_P_NOWAIT)
        // fork()の代用として呼ばれることが多いため、親(エミュレータ)は殺さずに並行動作させます
        intptr_t ret = _spawnvp(_P_NOWAIT, prog.c_str(), (char* const*)args.data());

        if (ret == -1) {
            std::cerr << "[Process] execlp failed: " << strerror(errno) << std::endl;
            return -1;
        }

        // 本来 exec は戻らない関数ですが、エミュレーションの都合上、成功時は0を返します
        return 0;
    }

    // ----------------------------------------------------------------
    // wait: Wait for process termination
    // ----------------------------------------------------------------
    // 現状は実装していませんが、必要に応じて _cwait 等を使用します
    /*
    int my_wait(int* status) {
        return _cwait(status, _getpid(), 0);
    }
    */
}