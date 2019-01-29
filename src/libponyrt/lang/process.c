#include <platform.h>
#include <stdio.h>
#include "process.h"

#ifdef PLATFORM_IS_WINDOWS

PONY_EXTERN_C_BEGIN

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

// https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-a-child-process-with-redirected-input-and-output

PONY_API size_t ponyint_win_process_create(
    char* appname,
    char* cmdline,
    char* environ,
    uint32_t stdin_fd, uint32_t stdout_fd, uint32_t stderr_fd)
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
        TRUE,        // handles are inherited // TODO: FALSE? since we pass the 3 directly
        0,           // creation flags
        environ,     // environment to use
        NULL,        // use parent's current directory
        &si,         // STARTUPINFO pointer
        &pi);        // receives PROCESS_INFORMATION

    if (success) {
        CloseHandle(pi.hThread);
        return (size_t) pi.hProcess;
    } else {
        return 0;
    }
}

// https://stackoverflow.com/questions/5487249/how-write-posix-waitpid-analog-for-windows

PONY_API int32_t ponyint_win_process_wait(size_t hProcess)
{
    if (WaitForSingleObject((HANDLE) hProcess, INFINITE) != 0) {
        return -1;
    }

    DWORD exit_code = -1;
    if (!GetExitCodeProcess((HANDLE) hProcess, &exit_code)) { // != 0 is good
        exit_code = GetLastError();
    }
    CloseHandle((HANDLE) hProcess);

    return exit_code;
}

PONY_API int32_t ponyint_win_process_kill(size_t hProcess)
{
    if (TerminateProcess((HANDLE) hProcess, 0)) {
        return 0;
    } else {
        return GetLastError();
    }
}

PONY_EXTERN_C_END

#endif // PLATFORM_IS_WINDOWS
