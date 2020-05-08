#include <stdio.h>
#include "process.h"

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_POSIX_BASED

#include <sys/wait.h>

PONY_API int32_t ponyint_wnohang() {
  return WNOHANG;
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
    if (!CreatePipe(&read_hnd, &write_hnd, &sa, 0)) {
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
 * Wait for a Windows process to complete, and return its exit code.
 * This does block if the process is still running.
 *
 * // https://stackoverflow.com/questions/5487249/how-write-posix-waitpid-analog-for-windows
 *
 * possible return values:
 * 0: all ok, extract the exitcode from exit_code_ptr
 * 1: process didnt finish yet
 * anything else: error waiting or getting the process exit code.
 */
PONY_API int32_t ponyint_win_process_wait(size_t hProcess, int32_t* exit_code_ptr)
{

    int32_t retval = 0;
    // just poll
    DWORD wait_result = WaitForSingleObject((HANDLE) hProcess, 0);
    if(wait_result == 0) {
      // process exited
      if (!GetExitCodeProcess((HANDLE) hProcess, (DWORD*)exit_code_ptr)) { // != 0 is good
          retval = GetLastError();
      }
    } else if(wait_result == 0x80) {
      // waiting timed out
      retval = 1;
    } else {
      // some other error
      retval = GetLastError();
    }

    CloseHandle((HANDLE) hProcess);

    return retval;
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
