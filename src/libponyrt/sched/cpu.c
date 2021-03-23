#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>
#include <dtrace.h>

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_BSD)
  #include <sched.h>
  #include <stdlib.h>
  #include <unistd.h>
  #include <stdio.h>
#elif defined(PLATFORM_IS_MACOSX)
  #include <unistd.h>
  #include <mach/mach.h>
#  if defined(PLATFORM_IS_ARM)
  #include <mach/mach_time.h>
#  endif
  #include <mach/thread_policy.h>
#elif defined(PLATFORM_IS_WINDOWS)
  #include <processtopologyapi.h>
#endif

#include "cpu.h"
#include "../mem/pool.h"

#if defined(PLATFORM_IS_LINUX)
static uint32_t avail_cpu_count;
static uint32_t* avail_cpu_list;
static uint32_t avail_cpu_size;
#endif

#if defined(PLATFORM_IS_MACOSX) || (defined(PLATFORM_IS_BSD) && !defined(PLATFORM_IS_OPENBSD))

#include <sys/types.h>
#include <sys/sysctl.h>

static uint32_t property(const char* key)
{
  int value = 0;
  size_t len = sizeof(int);
  int err = sysctlbyname(key, &value, &len, NULL, 0);
  return (err == 0 ? value : 0);
}
#endif

#if defined(PLATFORM_IS_OPENBSD)

static uint32_t cpus_online(void)
{
  int value = (int) sysconf(_SC_NPROCESSORS_ONLN);
  return value;
}

#endif

// Number of cores
static uint32_t hw_cpu_count;

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

static uint32_t cpu_add_mask_to_list(uint32_t i, cpu_set_t* mask)
{
  uint32_t count = CPU_COUNT(mask);
  uint32_t index = 0;
  uint32_t found = 0;

  while(found < count)
  {
    if(CPU_ISSET(index, mask))
    {
      avail_cpu_list[i++] = index;
      found++;
    }

    index++;
  }

  return i;
}
#endif

void ponyint_cpu_init()
{
#if defined(PLATFORM_IS_LINUX)
  cpu_set_t all_cpus;
  cpu_set_t hw_cpus;
  cpu_set_t ht_cpus;

  sched_getaffinity(0, sizeof(cpu_set_t), &all_cpus);
  CPU_ZERO(&hw_cpus);
  CPU_ZERO(&ht_cpus);

  avail_cpu_size = avail_cpu_count = CPU_COUNT(&all_cpus);
  uint32_t index = 0;
  uint32_t found = 0;

  while(found < avail_cpu_count)
  {
    if(CPU_ISSET(index, &all_cpus))
    {
      if(cpu_physical(index))
        CPU_SET(index, &hw_cpus);
      else
        CPU_SET(index, &ht_cpus);

      found++;
    }

    index++;
  }

  hw_cpu_count = CPU_COUNT(&hw_cpus);

  if(ponyint_numa_init())
  {
    uint32_t numa_count = ponyint_numa_cores();

    if(avail_cpu_count > numa_count)
      avail_cpu_size = numa_count;

    avail_cpu_list =
      (uint32_t*)ponyint_pool_alloc_size(avail_cpu_size * sizeof(uint32_t));
    avail_cpu_count =
      ponyint_numa_core_list(&hw_cpus, &ht_cpus, avail_cpu_list);
  } else {
    avail_cpu_list =
      (uint32_t*)ponyint_pool_alloc_size(avail_cpu_size * sizeof(uint32_t));

    uint32_t i = 0;
    i = cpu_add_mask_to_list(i, &hw_cpus);
    i = cpu_add_mask_to_list(i, &ht_cpus);
  }
#elif defined(PLATFORM_IS_FREEBSD)
  hw_cpu_count = property("kern.smp.cores");
  if (hw_cpu_count == 0) {
    hw_cpu_count = property("hw.ncpu");
  }
#elif defined(PLATFORM_IS_DRAGONFLY)
  hw_cpu_count = property("hw.cpu_topology_core_ids");  // # of real cores per package
  hw_cpu_count *= property("hw.cpu_topology_phys_ids"); // # of physical packages
  if (hw_cpu_count == 0) {
    hw_cpu_count = property("hw.ncpu");
  }
#elif defined(PLATFORM_IS_OPENBSD)
  hw_cpu_count = cpus_online();
#elif defined(PLATFORM_IS_BSD)
  hw_cpu_count = property("hw.ncpu");
#elif defined(PLATFORM_IS_MACOSX)
  hw_cpu_count = property("hw.physicalcpu");
#elif defined(PLATFORM_IS_WINDOWS)
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info = NULL;
  PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
  DWORD len = 0;
  DWORD offset = 0;

  while(true)
  {
    DWORD new_len = len;
    DWORD rc = GetLogicalProcessorInformation(info, &new_len);

    if(rc != FALSE)
      break;

    if(GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
      ptr = info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)
        ponyint_pool_realloc_size(len, new_len, ptr);
      len = new_len;
      continue;
    } else {
      if(ptr != NULL)
        ponyint_pool_free_size(len, ptr);

      return;
    }
  }

  while((offset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION)) <= len)
  {
    switch(info->Relationship)
    {
      case RelationProcessorCore:
        hw_cpu_count++;
        break;

      default: {}
    }

    offset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
    info++;
  }

  if(ptr != NULL)
    ponyint_pool_free_size(len, ptr);
#endif
}

uint32_t ponyint_cpu_count()
{
  return hw_cpu_count;
}

uint32_t ponyint_cpu_assign(uint32_t count, scheduler_t* scheduler,
  bool pin, bool pinasio)
{
  uint32_t asio_cpu = -1;

  if(!pin)
  {
    for(uint32_t i = 0; i < count; i++)
    {
      scheduler[i].cpu = -1;
      scheduler[i].node = 0;
    }

    return asio_cpu;
  }

#if defined(PLATFORM_IS_LINUX)
  for(uint32_t i = 0; i < count; i++)
  {
    uint32_t cpu = avail_cpu_list[i % avail_cpu_count];
    scheduler[i].cpu = cpu;
    scheduler[i].node = ponyint_numa_node_of_cpu(cpu);
  }

  // If pinning asio thread to a core is requested override the default
  // asio_cpu of -1
  if(pinasio)
    asio_cpu = avail_cpu_list[count % avail_cpu_count];

  ponyint_pool_free_size(avail_cpu_size * sizeof(uint32_t), avail_cpu_list);
  avail_cpu_list = NULL;
  avail_cpu_count = avail_cpu_size = 0;

  return asio_cpu;
#elif defined(PLATFORM_IS_BSD)
  // FreeBSD does not currently do thread pinning, as we can't yet determine
  // which cores are hyperthreads.

  // If pinning asio thread to a core is requested override the default
  // asio_cpu of -1
  if(pinasio)
    asio_cpu = count % hw_cpu_count;

  for(uint32_t i = 0; i < count; i++)
  {
    scheduler[i].cpu = i % hw_cpu_count;
    scheduler[i].node = 0;
  }
#else
  // Affinity groups rather than processor numbers.
  for(uint32_t i = 0; i < count; i++)
  {
    scheduler[i].cpu = i;
    scheduler[i].node = 0;
  }

  // If pinning asio thread to a core is requested override the default
  // asio_cpu of -1
  if(pinasio)
    asio_cpu = count;
#endif

  return asio_cpu;
}

void ponyint_cpu_affinity(uint32_t cpu)
{
  if(cpu == (uint32_t)-1)
    return;

#if defined(PLATFORM_IS_LINUX)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);

    sched_setaffinity(0, sizeof(cpu_set_t), &set);
#elif defined(PLATFORM_IS_BSD)
  // No pinning, since we cannot yet determine hyperthreads vs physical cores.
#elif defined(PLATFORM_IS_MACOSX)
  thread_affinity_policy_data_t policy;
  policy.affinity_tag = cpu;

  thread_policy_set(mach_thread_self(), THREAD_AFFINITY_POLICY,
    (thread_policy_t)&policy, THREAD_AFFINITY_POLICY_COUNT);
#elif defined(PLATFORM_IS_WINDOWS)
  GROUP_AFFINITY affinity;
  affinity.Mask = (uint64_t)1 << (cpu >> 6);
  affinity.Group = (cpu % 64);

  SetThreadGroupAffinity(ponyint_thread_self(), &affinity, NULL);
#endif
}

/**
 * Only nanosleep if sufficient cycles have elapsed.
 */
void ponyint_cpu_core_pause(uint64_t tsc, uint64_t tsc2, bool yield)
{
  // 10m cycles is about 3ms
  if((tsc2 - tsc) < 10000000)
    return;

#ifdef PLATFORM_IS_WINDOWS
  DWORD ts = 0;
#else
  struct timespec ts = {0, 0};
#endif

  if(yield)
  {
    // A billion cycles is roughly half a second, depending on clock speed.
    if((tsc2 - tsc) > 10000000000)
    {
      // If it has been 10 billion cycles, pause 30 ms.
#ifdef PLATFORM_IS_WINDOWS
      ts = 30;
#else
      ts.tv_nsec = 30000000;
#endif
    } else if((tsc2 - tsc) > 3000000000) {
      // If it has been 3 billion cycles, pause 10 ms.
#ifdef PLATFORM_IS_WINDOWS
      ts = 10;
#else
      ts.tv_nsec = 10000000;
#endif
    } else if((tsc2 - tsc) > 1000000000) {
      // If it has been 1 billion cycles, pause 1 ms.
#ifdef PLATFORM_IS_WINDOWS
      ts = 1;
#else
      ts.tv_nsec = 1000000;
#endif
    }
    else
    {
#ifdef PLATFORM_IS_WINDOWS
      // Otherwise, pause for 1 ms (minimum on windows)
      ts = 1;
#else
      // Otherwise, pause for 100 microseconds
      ts.tv_nsec = 100000;
#endif
    }
  }

#ifdef PLATFORM_IS_WINDOWS
  Sleep(ts);
#else
  DTRACE1(CPU_NANOSLEEP, ts.tv_nsec);
  nanosleep(&ts, NULL);
#endif
}

void ponyint_cpu_relax()
{
#if defined(PLATFORM_IS_X86) && !defined(PLATFORM_IS_VISUAL_STUDIO)
  asm volatile("pause" ::: "memory");
#endif
}

uint64_t ponyint_cpu_tick()
{
#if defined PLATFORM_IS_ARM
# if defined(__APPLE__)
  return mach_absolute_time();
# else
#   if defined ARMV6
  // V6 is the earliest arch that has a standard cyclecount
  uint32_t pmccntr;
  uint32_t pmuseren;
  uint32_t pmcntenset;

  // Read the user mode perf monitor counter access permissions.
  asm volatile ("mrc p15, 0, %0, c9, c14, 0" : "=r" (pmuseren));

  // Allows reading perfmon counters for user mode code.
  if(pmuseren & 1)
  {
    asm volatile ("mrc p15, 0, %0, c9, c12, 1" : "=r" (pmcntenset));

    // Is it counting?
    if(pmcntenset & 0x80000000ul)
    {
      // The counter is set up to count every 64th cycle
      asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (pmccntr));
      return pmccntr << 6;
    }
  }
#   endif

#   if defined(PLATFORM_IS_LINUX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  return (ts.tv_sec * 1000000000) + ts.tv_nsec;
#   else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000000000) + ts.tv_nsec;
#   endif
# endif
#elif defined PLATFORM_IS_X86
# if defined(PLATFORM_IS_CLANG_OR_GCC)
#   ifdef __clang__
  return __builtin_readcyclecounter();
#   else
  return __builtin_ia32_rdtsc();
#   endif
# elif defined(PLATFORM_IS_WINDOWS)
  return __rdtsc();
# endif
#endif
}
