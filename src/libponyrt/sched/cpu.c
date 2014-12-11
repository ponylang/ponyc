#define _GNU_SOURCE
#include <platform.h>
#if defined(PLATFORM_IS_LINUX)
#include <numa.h>
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

#if defined(PLATFORM_IS_LINUX)
static bool cpu_physical(uint32_t cpu)
{
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

  return true;
}
#endif

uint32_t cpu_count()
{
#if defined(PLATFORM_IS_LINUX)
  if(numa_available())
  {
    struct bitmask* cpus = numa_get_run_node_mask();
    return numa_bitmask_weight(cpus);
  }

  uint32_t max = sysconf(_SC_NPROCESSORS_ONLN);
  uint32_t count = 0;

  for(uint32_t i = 0; i < max; i++)
  {
    if(cpu_physical(i))
      count++;
  }

  return count;
#elif defined(PLATFORM_IS_MACOSX)
  return property("hw.physicalcpu");
#elif defined(PLATFORM_IS_WINDOWS)
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info = NULL;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
  DWORD len = 0;
  DWORD offset = 0;
  uint32_t count = 0;

  while(true)
  {
    DWORD rc = GetLogicalProcessorInformation(info, &len);

    if(rc != FALSE)
      break;

    if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
      ptr = info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(len);
      continue;
    } else {
      return 0;
    }
  }

  while((offset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)) <= len)
  {
    switch(info->Relationship)
    {
      case RelationProcessorCore:
        count++;
        break;

      default: {}
    }

    offset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    info++;
  }

  if(ptr != NULL)
    free(ptr);

  return count;
#endif
}

void cpu_assign(uint32_t count, scheduler_t* scheduler)
{
#if defined(PLATFORM_IS_LINUX)
  if(numa_available())
  {
    // Assign only numa-available cores.
    struct bitmask* cpus = numa_get_run_node_mask();
    uint32_t max = numa_bitmask_nbytes(cpus) * 8;

    uint32_t core = 0;
    uint32_t thread = 0;

    while(thread < count)
    {
      if(numa_bitmask_isbitset(cpus, core))
        scheduler[thread++].cpu = core;

      core++;

      // Wrap around if we have more threads than cores.
      if(core >= max)
        core = 0;
    }
  } else {
    // Physical cores come first, so assign in sequence.
    uint32_t max = sysconf(_SC_NPROCESSORS_ONLN);

    for(uint32_t i = 0; i < count; i++)
      scheduler[i].cpu = i % max;
  }
#else
  // Affinity groups rather than processor numbers.
  for(uint32_t i = 0; i < count; i++)
    scheduler[i].cpu = i;
#endif
}

void cpu_affinity(uint32_t cpu)
{
#if defined(PLATFORM_IS_LINUX)
  // Affinity is handled when spawning the thread.
  (void)cpu;

  // Allocate memory on the local node.
  if(numa_available())
    numa_set_localalloc();
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
