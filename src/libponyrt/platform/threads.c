#include <platform.h>

bool pony_thread_create(pony_thread_id_t* thread, thread_fn start, void* arg)
{
#ifdef PLATFORM_IS_WINDOWS
  uintptr_t p = _beginthreadex(NULL, 0, start, arg, 0, NULL);

  if(!p)
    return false;

  *thread = (HANDLE)p;
#else
  if(pthread_create(thread, NULL, start, arg))
    return false;
#endif
  return true;
}

bool pony_thread_join(pony_thread_id_t thread)
{
#ifdef PLATFORM_IS_WINDOWS
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
#else
  if(pthread_join(thread, NULL))
    return false;
#endif

  return true;
}

void pony_thread_detach(pony_thread_id_t thread)
{
#ifndef PLATFORM_IS_WINDOWS
  pthread_detach(thread);
#endif
}

pony_thread_id_t pony_thread_self()
{
#ifdef PLATFORM_IS_WINDOWS
  return GetCurrentThread();
#else
  return pthread_self();
#endif
}
