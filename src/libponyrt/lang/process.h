#ifndef lang_process_h
#define lang_process_h

#include <platform.h>

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_WINDOWS

PONY_API uint32_t ponyint_win_pipe_create(uint32_t* near_fd, uint32_t* far_fd,
    bool outgoing);

PONY_API size_t ponyint_win_process_create(char* appname, char* cmdline,
    char* environ, char* wdir, uint32_t stdin_fd, uint32_t stdout_fd,
    uint32_t stderr_fd, uint32_t* error_code, char** error_msg);

PONY_API int32_t ponyint_win_process_wait(size_t hProcess);

PONY_API int32_t ponyint_win_process_kill(size_t hProcess);

#elif defined(PLATFORM_IS_POSIX_BASED)

PONY_API int32_t ponyint_wnohang();

#endif

PONY_EXTERN_C_END

#endif
