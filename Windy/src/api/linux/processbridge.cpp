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
    int my_fork(void) {
        std::cout << "[Process] fork called. Pretending to be Parent (PID=1000)." << std::endl;
        return 1000;
    }

    // ----------------------------------------------------------------
    // vfork: Create a child process and block parent
    // ----------------------------------------------------------------
    int my_vfork(void) {
        return my_fork();
    }

    // ----------------------------------------------------------------
    // daemon: Run in background
    // ----------------------------------------------------------------
    int my_daemon(int nochdir, int noclose) {
        std::cout << "[Process] daemon called. Success." << std::endl;
        return 0;
    }

    // ----------------------------------------------------------------
    // execlp: Execute a file
    // ----------------------------------------------------------------
    int my_execlp(const char* file, const char* arg, ...) {
        std::cout << "[Process] execlp called: " << file << std::endl;

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
        args.push_back(NULL);

        // Path adjustment 
        std::string prog = file;
        if (prog == "sh" || prog == "/bin/sh") {
            prog = "C:\\msys64\\usr\\bin\\sh.exe";
        }

        intptr_t ret = _spawnvp(_P_NOWAIT, prog.c_str(), (char* const*)args.data());

        if (ret == -1) {
            std::cerr << "[Process] execlp failed: " << strerror(errno) << std::endl;
            return -1;
        }

        return 0;
    }

    // ----------------------------------------------------------------
    // wait: Wait for process termination
    // ----------------------------------------------------------------
    // 
    /*
    int my_wait(int* status) {
        return _cwait(status, _getpid(), 0);
    }
    */
}