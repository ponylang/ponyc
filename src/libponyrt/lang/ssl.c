#include <platform.h>
#include "../mem/pool.h"

#if defined(PLATFORM_IS_WINDOWS)
static HANDLE* locks;
#else
static pthread_mutex_t* locks;
#endif


PONY_EXTERN_C_BEGIN

// Forward declaration to avoid C++ name mangling
PONY_API void* ponyint_ssl_multithreading(uint32_t count);

PONY_EXTERN_C_END


static void locking_callback(int mode, int type, const char* file, int line)
{
  (void)file;
  (void)line;

  if(mode & 1)
  {
#if defined(PLATFORM_IS_WINDOWS)
    // Wait for the lock, but stay in an "alertable" state so that APCs can run.
    // Things like socket I/O depend on being able to run APCs on the thread.
    // If we wake due to an APC (instead of the lock), wait some more.
    while (
      WaitForSingleObjectEx(locks[type], INFINITE, true) == WAIT_IO_COMPLETION
    );
#else
    pthread_mutex_lock(&locks[type]);
#endif
  } else {
#if defined(PLATFORM_IS_WINDOWS)
    ReleaseMutex(locks[type]);
#else
    pthread_mutex_unlock(&locks[type]);
#endif
  }
}

PONY_API void* ponyint_ssl_multithreading(uint32_t count)
{
#if defined(PLATFORM_IS_WINDOWS)
  locks = (HANDLE*)ponyint_pool_alloc_size(count * sizeof(HANDLE*));

  for(uint32_t i = 0; i < count; i++)
    locks[i] = CreateMutex(NULL, FALSE, NULL);
#else
  locks = (pthread_mutex_t*)
    ponyint_pool_alloc_size(count * sizeof(pthread_mutex_t));

  for(uint32_t i = 0; i < count; i++)
    pthread_mutex_init(&locks[i], NULL);
#endif

  return locking_callback;
}
