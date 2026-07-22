#include <stdio.h>
#include "process.h"

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_POSIX_BASED

#include <sys/wait.h>

PONY_API int32_t ponyint_wnohang() {
  return WNOHANG;
}
#endif

#ifdef PLATFORM_IS_LINUX

#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>

// Open a pidfd for a process. A pidfd goes readable when the process exits and
// registers with epoll like any other fd, giving exit detection that does not
// depend on the child's pipes closing. Returns the fd, or -1 with errno set;
// errno ENOSYS means the kernel is older than 5.3 and does not have the
// syscall. We call the syscall directly rather than glibc's pidfd_open wrapper,
// which only exists in glibc 2.36+; SYS_pidfd_open comes from the kernel
// headers. If the build-time headers predate the syscall number, we report it
// as unsupported.
PONY_API int32_t ponyint_pidfd_open(int32_t pid) {
#ifdef SYS_pidfd_open
  return (int32_t)syscall(SYS_pidfd_open, (pid_t)pid, 0u);
#else
  errno = ENOSYS;
  return -1;
#endif
}
#endif

#ifdef PLATFORM_IS_WINDOWS



/**
 * Create an anonymous pipe for communicating with another (most likely child)
 * process. Create and return C FDs for the handles and return those via the
 * uint32 ptrs provided. The outgoing flag determines whether the pipe handles
 * will be oriented for outgoing (near=write) or incoming (near=read).
 * Return true if OK, false if not.
 *
 * This MS Doc is the most generally useful reference on working with
 * child processes and pipes:
 *   https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-a-child-process-with-redirected-input-and-output
 *
 * Also useful is that anonymous pipes (for child processes in/out/err) are
 * just named pipes with special names, and can use the same functions:
 *   https://msdn.microsoft.com/en-us/library/windows/desktop/aa365787(v=vs.85).aspx
 */
PONY_API uint32_t ponyint_win_pipe_create(uint32_t* near_fd, uint32_t* far_fd, bool outgoing) {
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;  // default to inherit, clear the flag on the near hnd later

    HANDLE read_hnd;
    HANDLE write_hnd;
    // A stated buffer size. A write to a PIPE_NOWAIT pipe writes all of the
    // request or none of it, so a write larger than the buffer writes nothing
    // (unless a reader is blocked in a read to hand it to, which a peeking
    // reader is not). _Pipe.write caps each write to this size, so a write that
    // would not fit becomes writes that do. The 65536 there and here must
    // match.
    if (!CreatePipe(&read_hnd, &write_hnd, &sa, 65536)) {
        return false;
    }

    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
    if (outgoing) {
        SetHandleInformation(write_hnd, HANDLE_FLAG_INHERIT, 0);
        SetNamedPipeHandleState(write_hnd, &mode, NULL, NULL);
        *near_fd = _open_osfhandle((intptr_t)write_hnd, 0);
        *far_fd = _open_osfhandle((intptr_t)read_hnd, 0);
    } else {
        SetHandleInformation(read_hnd, HANDLE_FLAG_INHERIT, 0);
        SetNamedPipeHandleState(read_hnd, &mode, NULL, NULL);
        *near_fd = _open_osfhandle((intptr_t)read_hnd, 0);
        *far_fd = _open_osfhandle((intptr_t)write_hnd, 0);
    }
    return true;
}

/**
 * Create a Windows child process given an app name, a command line, and
 * environment variable list, and the three std file FDs.
 * Return the Windows Handle of the new process, which always fits in a size_t.
 */
PONY_API size_t ponyint_win_process_create(
    char* appname,
    char* cmdline,
    char* environ,
    char* wdir,
    uint32_t stdin_fd,
    uint32_t stdout_fd,
    uint32_t stderr_fd,
    uint32_t* error_code,
    char** error_message)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdInput = (HANDLE) _get_osfhandle(stdin_fd);
    si.hStdOutput = (HANDLE) _get_osfhandle(stdout_fd);
    si.hStdError = (HANDLE) _get_osfhandle(stderr_fd);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcess(
        appname,
        cmdline,     // command line
        NULL,        // process security attributes
        NULL,        // primary thread security attributes
        TRUE,        // handles are inherited
        0,           // creation flags
        environ,     // environment to use
        wdir,        // current directory of the process
        &si,         // STARTUPINFO pointer
        &pi);        // receives PROCESS_INFORMATION

    if (success) {
        // Close the thread handle now as we don't need access to it
        CloseHandle(pi.hThread);
        *error_code = 0;
        return (size_t) pi.hProcess;
    } else {
        *error_code = GetLastError();
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER
            | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, *error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)error_message, 0, NULL);
        return 0;  // Null handle return on non-success.
    }
}

/**
 * Report a Windows child's exit status without blocking. Does not close the
 * process handle: the asio backend registers a wait on that handle for the
 * exit event, and the handle is closed once, later, when that wait is
 * unregistered (see the process package's exit-source teardown). Closing it
 * here while the wait is registered is undefined behavior.
 *
 * WaitForSingleObject with a zero timeout is the non-blocking poll:
 * WAIT_OBJECT_0 means the child has exited, so a real exit code of 259 is not
 * confused with the STILL_ACTIVE that GetExitCodeProcess returns for a running
 * process; WAIT_TIMEOUT means it is still running.
 *
 * Return values:
 *   0: exited; read the exit code from exit_code_ptr.
 *   1: still running.
 *  -1: error waiting or reading the exit code. A fixed sentinel, never a raw
 *      Windows error code, so an error can never collide with the 1 that means
 *      still-running.
 */
PONY_API int32_t ponyint_win_process_wait(size_t hProcess, int32_t* exit_code_ptr)
{
    switch (WaitForSingleObject((HANDLE)hProcess, 0))
    {
        case WAIT_OBJECT_0: // process exited
            if (GetExitCodeProcess((HANDLE)hProcess, (DWORD*)exit_code_ptr) == 0)
                return -1;
            return 0;
        case WAIT_TIMEOUT: // process is still running
            return 1;
        default: // WAIT_ABANDONED (shouldn't happen to a process), WAIT_FAILED
            return -1;
    }
}

/**
 * Terminate a Windows process. Return 0 if OK, otherwise the Windows error
 * code.
 */
PONY_API int32_t ponyint_win_process_kill(size_t hProcess)
{
    if (TerminateProcess((HANDLE) hProcess, 0)) {
        return 0;
    } else {
        return GetLastError();
    }
}
#endif // PLATFORM_IS_WINDOWS

PONY_EXTERN_C_END
