#define _GNU_SOURCE
#include <platform.h>
#if defined(PLATFORM_IS_LINUX)
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#elif defined(PLATFORM_IS_MACOSX)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>
#elif defined(PLATFORM_IS_WINDOWS)
#include <processtopologyapi.h>
#endif

#include "cpu.h"

#if defined(PLATFORM_IS_MACOSX)
static uint32_t property(const char* key)
{
  int value;
  size_t len = sizeof(int);
  sysctlbyname(key, &value, &len, NULL, 0);
  return value;
}
#endif

void cpu_count(uint32_t* physical, uint32_t* logical)
{
#if defined(PLATFORM_IS_LINUX)
  int count = (int)sysconf(_SC_NPROCESSORS_ONLN);
  int res = 0;

  for(int i = 0; i < count; i++)
  {
    if(cpu_physical(i))
      res++;
  }

  *physical = res;
  *logical = count;
#elif defined(PLATFORM_IS_MACOSX)
  *physical = property("hw.physicalcpu");
  *logical = property("hw.logicalcpu");
#elif defined(PLATFORM_IS_WINDOWS)
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info = NULL;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
  DWORD len = 0;
  DWORD offset = 0;

  while(true)
  {
    DWORD rc = GetLogicalProcessorInformation(info, &len);

    if(rc != FALSE)
      break;

    if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
      ptr = info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(len);
      continue;
    }
    else
    {
      *physical = 0;
      *logical = 0;

      return;
    }
  }

  while(offset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= len)
  {
    switch(info->Relationship)
    {
      case RelationProcessorCore:
        *physical += 1;
        *logical += (uint32_t)__pony_popcount64(info->ProcessorMask);
        break;

      default: {}
    }

    offset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    info++;
  }

  free(ptr);
#endif
}

bool cpu_physical(uint32_t cpu)
{
#if defined(PLATFORM_IS_LINUX)
  char file[FILENAME_MAX];
  snprintf(file, FILENAME_MAX,
    "/sys/devices/system/cpu/cpu%d/topology/thread_siblings_list", cpu);

  FILE* fp = fopen(file, "r");

  if(fp != NULL)
  {
    char name[16];
    size_t len = fread(name, 1, 15, fp);
    name[len] = '\0';
    fclose(fp);

    if(cpu != (uint32_t)atoi(name))
      return false;
  }
#else
  (void)cpu;
#endif

  return true;
}

void cpu_affinity(uint32_t cpu)
{
#if defined(PLATFORM_IS_LINUX)
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);

  sched_setaffinity(0, 1, &set);
#elif defined(PLATFORM_IS_MACOSX)
  thread_affinity_policy_data_t policy;
  policy.affinity_tag = cpu;

  thread_policy_set(mach_thread_self(), THREAD_AFFINITY_POLICY,
    (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);
#elif defined(PLATFORM_IS_WINDOWS)
  GROUP_AFFINITY affinity;
  affinity.Mask = (uint64_t)1 << (cpu >> 6);
  affinity.Group = (cpu % 64);

  SetThreadGroupAffinity(pony_thread_self(), &affinity, NULL);
#endif
}

uint64_t cpu_rdtsc()
{
  return __pony_rdtsc();
}

/**
 * Only nanosleep if sufficient cycles have elapsed.
 */
bool cpu_core_pause(uint64_t tsc)
{
  uint64_t tsc2 = cpu_rdtsc();

  // 10m cycles is about 3ms
  if((tsc2 - tsc) < 10000000)
    return false;

#ifndef PLATFORM_IS_WINDOWS
  struct timespec ts = {0, 0};
  nanosleep(&ts, NULL);
#else
  Sleep(0);
#endif

  return true;
}
