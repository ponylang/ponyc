#ifndef lang_process_h
#define lang_process_h

PONY_EXTERN_C_BEGIN

#ifdef PLATFORM_IS_WINDOWS

PONY_API uint32_t ponyint_win_pipe_create(uint32_t* near_fd, uint32_t* far_fd, bool outgoing);

PONY_API size_t ponyint_win_process_create(char* appname, char* cmdline,
    char* environ, uint32_t stdin_fd, uint32_t stdout_fd, uint32_t stderr_fd);

PONY_API int32_t ponyint_win_process_wait(size_t hProcess);

PONY_API int32_t ponyint_win_process_kill(size_t hProcess);

#endif

PONY_EXTERN_C_END

#endif
