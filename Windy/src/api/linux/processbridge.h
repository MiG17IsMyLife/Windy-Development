#pragma once

// =============================================================
//   Process Control Emulation (extern "C")
// =============================================================
extern "C"
{
    // Fork a new process
    // Windows does not support fork(), so this implementation
    // usually returns a positive integer to simulate being the
    // parent process, skipping the child code block.
    int my_fork(void);

    // Vfork (same as fork)
    int my_vfork(void);

    // Run in the background
    // nochdir: if 0, change CWD to /
    // noclose: if 0, redirect stdin/out/err to /dev/null
    // In emulation, this function often does nothing and returns success.
    int my_daemon(int nochdir, int noclose);

    // Execute a file
    // Replaces the current process image with a new process image.
    // In emulation, this typically launches a separate process via _spawn.
    int my_execlp(const char *file, const char *arg, ...);

    // Wait for process termination (if needed)
    // int my_wait(int* status);
}