#define _CRT_SECURE_NO_WARNINGS
#include "processbridge.h"
#include "../../core/log.h"
#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <process.h>
#include <stdarg.h>
#include <errno.h>

extern "C"
{

    int my_fork(void)
    {
        log_debug("fork() called - returning fake parent PID 1000");
        return 1000;
    }

    int my_vfork(void)
    {
        log_debug("vfork() called - returning fake parent PID 1000");
        return my_fork();
    }

    int my_daemon(int nochdir, int noclose)
    {
        log_debug("daemon(%d, %d) called - stubbed success", nochdir, noclose);
        return 0;
    }

    int my_execlp(const char *file, const char *arg, ...)
    {
        log_info("execlp(\"%s\", \"%s\", ...)", file, arg);

        std::vector<const char *> args;
        args.push_back(arg);

        va_list ap;
        va_start(ap, arg);
        while (1)
        {
            const char *p = va_arg(ap, const char *);
            if (p == NULL)
                break;
            args.push_back(p);
        }
        va_end(ap);
        args.push_back(NULL);

        std::string prog = file;
        if (prog == "sh" || prog == "/bin/sh")
        {
            prog = "C:\\msys64\\usr\\bin\\sh.exe";
        }

        intptr_t ret = _spawnvp(_P_NOWAIT, prog.c_str(), (char *const *)args.data());

        if (ret == -1)
        {
            log_error("execlp failed: %s", strerror(errno));
            return -1;
        }

        log_debug("execlp: Spawned process %s", prog.c_str());
        return 0;
    }
}