#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

PONY_EXTERN_C_BEGIN

#if defined(PLATFORM_IS_POSIX_BASED)
#include <limits.h>
#include <unistd.h>
#elif defined(PLATFORM_IS_WINDOWS)
#include <io.h>
#endif


// How many buffers a single gather-write may carry. Read by both packages/net
// (TCPConnection) and packages/files (File). On Windows net's pony_os_sendv
// does a real WSASend over a WSABUF array, but file.pony's Windows write branch
// does a single-buffer write keyed off this same cap: raising it above 1 there
// would hand file.pony a multi-buffer total and it would over-read past the
// first buffer. Keep the Windows cap at 1 unless both consumers are updated.
PONY_API int pony_os_writev_max()
{
#if defined(PLATFORM_IS_POSIX_BASED)
  return IOV_MAX;
#else
  return 1;
#endif
}

PONY_API int64_t pony_os_lseek(int fd, int64_t offset, int whence)
{
#ifdef PLATFORM_IS_LINUX
#ifdef __GLIBC__
  return lseek64(fd, offset, whence);
#else
  return lseek(fd, offset, whence);
#endif
#elif defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_BSD) || defined(PLATFORM_IS_HAIKU)
  return lseek(fd, offset, whence);
#elif defined(PLATFORM_IS_WINDOWS)
  return _lseeki64(fd, offset, whence);
#endif
}

PONY_API int pony_os_ftruncate(int fd, int64_t length)
{
#ifdef PLATFORM_IS_LINUX
#ifdef __GLIBC__
  return ftruncate64(fd, length);
#else
  return ftruncate(fd, length);
#endif
#elif defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_BSD) || defined(PLATFORM_IS_HAIKU)
  return ftruncate(fd, length);
#elif defined(PLATFORM_IS_WINDOWS)
  return _chsize_s(fd, length);
#endif
}

PONY_EXTERN_C_END
