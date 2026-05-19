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
#elif defined(PLATFORM_IS_EMSCRIPTEN)
  #include <emscripten.h>
  #include <emscripten/threading.h>
#elif defined(PLATFORM_IS_HAIKU)
  #include <sched.h>
  #include <os/kernel/OS.h>
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

#if defined(PLATFORM_IS_HAIKU)

// Haiku supports up to 64 CPUs anyway, so let's make our life easier,
// and void alloc/free multiple times.
typedef unsigned long long cpu_map;
const uint8 MAX_CPU = sizeof(cpu_map) * CHAR_BIT;
static cpu_map cpu_cores = 0;
static cpu_map cpu_smts = 0;

static uint32_t haiku_cpu_count(void)
{
  system_info system;
  if(get_system_info(&system) < B_OK)
  {
    cpu_cores = 1;
    cpu_smts = 1;
    return 1; // Bail early, since if this didn't work, everything is broken.
  }

  // `system_info.cpu_count` reflects all SMTs, not just cores or physical dies.
  // But it does not care if some of them are disabled at the moment.

  // Get info about all cpus to know which ones are enabled at the moment.
  cpu_info info[MAX_CPU];
  if(get_cpu_info(0, MAX_CPU, info) != B_OK)
    goto failure;

  // Check how many topology nodes there will be.
  uint32 topology_count = 0;
  if(get_cpu_topology_info(NULL, &topology_count) != B_OK)
    goto failure;

  uint32 cpus_size = topology_count * sizeof(cpu_topology_node_info);
  cpu_topology_node_info *cpus =
    (cpu_topology_node_info*)ponyint_pool_alloc_size(cpus_size);
  if(cpus == NULL)
    goto failure;

  // Get all topology nodes.
  if(get_cpu_topology_info(cpus, &topology_count) != B_OK)
  {
    ponyint_pool_free_size(cpus_size, cpus);
    goto failure;
  }

  // Topology goes like this:
  // - ROOT
  // -- PACKAGE
  // --- CORE
  // ---- SMT
  // PACKAGE is a CPU die, so a "socket" in QEmu.
  // Every CORE has at least one SMT node (AFAIK).
  // We treat every first SMT's Id of core as a "core_id", others "more_id".
  bool next_smt_is_core = false;
  for(uint32 i = 0; i < topology_count; i++)
  {
    if(cpus[i].type == B_TOPOLOGY_CORE)
    {
      next_smt_is_core = true;
    }
    else if(cpus[i].type == B_TOPOLOGY_SMT)
    {
      if(cpus[i].id >= MAX_CPU)
        break;
      if(!info[cpus[i].id].enabled)
      {
        // Ignore disabled cpus.
        // TODO: find a way to observe dynamically when cpu gets (re)enabled?
      }
      else if(next_smt_is_core)
      {
        cpu_cores |= 1L << cpus[i].id;
        next_smt_is_core = false;
      }
      else
      {
        cpu_smts |= 1L << cpus[i].id;
      }
    }
  }

  ponyint_pool_free_size(cpus_size, cpus);

  // success:
  return __builtin_popcountll(cpu_cores);

  failure:
  for(uint8 i = 0; i < system.cpu_count; i++)
  {
    cpu_cores |= 1LL << i;
  }
  cpu_smts = cpu_cores;
  return system.cpu_count;
}

static uint8 next_cpu_id(uint8 *last_pos, cpu_map *cpus, bool *cpus_are_cores)
{
  if(*cpus == 0)
  {
    *last_pos = 0;
    *cpus_are_cores = !(*cpus_are_cores);
    *cpus = *cpus_are_cores ? cpu_smts : cpu_cores;
  }
  uint8 pos = (uint8)__builtin_ctzll(*cpus) + 1;
  *cpus = *cpus >> pos;
  *last_pos += pos;
  return *last_pos - 1;
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
#elif defined(PLATFORM_IS_EMSCRIPTEN)
  hw_cpu_count = emscripten_num_logical_cores();
#elif defined(PLATFORM_IS_HAIKU)
  hw_cpu_count = haiku_cpu_count();
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
#else
  #warning Missing hw_cpu_count on unsupported platform
  hw_cpu_count = 1;
#endif
}

uint32_t ponyint_cpu_count()
{
  return hw_cpu_count;
}

uint32_t ponyint_cpu_assign(uint32_t count, scheduler_t* scheduler,
  bool pin, bool pinasio, bool pinpat, bool pin_tracing_thread, uint32_t* tracing_cpu)
{
  uint32_t asio_cpu = -1;
  uint32_t pat_cpu = -1;
  *tracing_cpu = -1;

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

  if(pinpat)
    pat_cpu = avail_cpu_list[(count + 1) % avail_cpu_count];

  if(pin_tracing_thread)
    *tracing_cpu = avail_cpu_list[(count + 2) % avail_cpu_count];

  ponyint_pool_free_size(avail_cpu_size * sizeof(uint32_t), avail_cpu_list);
  avail_cpu_list = NULL;
  avail_cpu_count = avail_cpu_size = 0;
#elif defined(PLATFORM_IS_BSD)
  // FreeBSD does not currently do thread pinning, as we can't yet determine
  // which cores are hyperthreads.

  // If pinning asio thread to a core is requested override the default
  // asio_cpu of -1
  if(pinasio)
    asio_cpu = count % hw_cpu_count;

  if(pinpat)
    pat_cpu = (count + 1) % hw_cpu_count;

  if(pin_tracing_thread)
    *tracing_cpu = (count + 2) % hw_cpu_count;

  for(uint32_t i = 0; i < count; i++)
  {
    scheduler[i].cpu = i % hw_cpu_count;
    scheduler[i].node = 0;
  }
#elif defined(PLATFORM_IS_HAIKU)
  uint8 cookie = 0;
  cpu_map curr_map = cpu_cores;
  bool curr_are_cores = true;

  for(uint32_t i = 0; i < count; i++)
  {
    scheduler[i].cpu = next_cpu_id(&cookie, &curr_map, &curr_are_cores);
    scheduler[i].node = 0;
  }

  // If pinning asio thread to a core is requested override the default
  // asio_cpu of -1
  if(pinasio)
    asio_cpu = next_cpu_id(&cookie, &curr_map, &curr_are_cores);

  if(pinpat)
    pat_cpu = next_cpu_id(&cookie, &curr_map, &curr_are_cores);

  if(pin_tracing_thread)
    *tracing_cpu = next_cpu_id(&cookie, &curr_map, &curr_are_cores);
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

  if(pinpat)
    pat_cpu = (count + 1);

  if(pin_tracing_thread)
    *tracing_cpu = (count + 2);
#endif

  // set the affinity of the current thread (nain thread) which is the pinned
  // actor thread
  ponyint_cpu_affinity(pat_cpu);

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
#elif defined(PLATFORM_IS_HAIKU)
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);

  sched_setaffinity(0, sizeof(cpu_set_t), &set);
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
  uint64_t diff = ponyint_cpu_tick_diff(tsc, tsc2);

  // 10m cycles is about 3ms
  if(diff < 10000000)
    return;

  if(yield)
  {
#ifdef PLATFORM_IS_WINDOWS
    DWORD ts = 0;
#else
    struct timespec ts = {0, 0};
#endif

    // A billion cycles is roughly half a second, depending on clock speed.
    if(diff > 10000000000)
    {
      // If it has been 10 billion cycles, pause 30 ms.
#ifdef PLATFORM_IS_WINDOWS
      ts = 30;
#else
      ts.tv_nsec = 30000000;
#endif
    } else if(diff > 3000000000) {
      // If it has been 3 billion cycles, pause 10 ms.
#ifdef PLATFORM_IS_WINDOWS
      ts = 10;
#else
      ts.tv_nsec = 10000000;
#endif
    } else if(diff > 1000000000) {
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
      // Windows minimum for pause is 1 millisecond which is a long time
      // compared to what we want. Instead, let's not yield. But to go a little
      // too fast then 10x too slow for waking up.
      ts = 0;
#else
      // Otherwise, pause for 100 microseconds
      ts.tv_nsec = 100000;
#endif
    }

#ifdef PLATFORM_IS_WINDOWS
    SleepEx(ts, true);
#else
    DTRACE1(CPU_NANOSLEEP, ts.tv_nsec);
    nanosleep(&ts, NULL);
#endif
  }
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
  return clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
# elif defined(PLATFORM_IS_WINDOWS)
  LARGE_INTEGER PerformanceCount;
  QueryPerformanceCounter(&PerformanceCount);
  return PerformanceCount.QuadPart;
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
      // Widen before shifting: pmccntr is 32 bits, so a bare `pmccntr << 6`
      // is 32-bit arithmetic and discards the top 6 bits, wrapping every 2^32
      // cycles. The cast keeps all 38 significant bits (32-bit counter scaled
      // by 64). This is the only ponyint_cpu_tick() path that wraps within a
      // process lifetime; PONY_CPU_TICK_BITS below and ponyint_cpu_tick_diff()
      // depend on this 38-bit width, so keep them in sync with this shift.
      return ((uint64_t)pmccntr) << 6;
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
#elif defined PLATFORM_IS_EMSCRIPTEN
  return (uint64_t)(emscripten_get_now() * 1e6);
#elif defined PLATFORM_IS_RISCV
  uint64_t cycles;
  asm("rdcycle %0" : "=r"(cycles));
  return cycles;
#endif
}

// Width, in bits, of the value ponyint_cpu_tick() returns. Every path is an
// effectively non-wrapping 64-bit value (rdtsc, clock_gettime nanoseconds,
// QueryPerformanceCounter, rdcycle, emscripten) except the AArch32 hardware
// PMU path, which returns a 32-bit PMCCNTR scaled to 38 bits and wraps every
// 2^38 cycles (a couple of minutes at GHz clocks). This condition must stay
// byte-identical to the one guarding the `pmccntr` return in
// ponyint_cpu_tick(): if it drifts, diffs are masked at the wrong width.
#if defined PLATFORM_IS_ARM && !defined(__APPLE__) && \
  !defined(PLATFORM_IS_WINDOWS) && defined ARMV6
#  define PONY_CPU_TICK_BITS 38
#else
#  define PONY_CPU_TICK_BITS 64
#endif

// Elapsed ticks between two ponyint_cpu_tick() readings, correct across a
// single wrap of a `bits`-wide counter. For a 64-bit counter the unsigned
// subtraction is already correct modulo 2^64, so we return it directly (and
// avoid the undefined `1 << 64`). For a narrower counter we take the result
// modulo 2^bits, which recovers the true elapsed value as long as it is
// smaller than 2^bits. A longer interval reads as its value modulo 2^bits; see
// ponyint_cpu_tick_diff for the one place that happens and why it stays
// harmless.
uint64_t ponyint_cpu_tick_diff_bits(uint64_t supposedly_earlier,
  uint64_t supposedly_later, uint32_t bits)
{
  uint64_t raw = supposedly_later - supposedly_earlier;

  if(bits >= 64)
    return raw;

  return raw & ((UINT64_C(1) << bits) - 1);
}

// The wrapping cycle counter is the AArch32 PMU path in ponyint_cpu_tick(),
// not the `clock_gettime` fallback: the nanosecond clock is 64 bits and would
// take centuries to wrap. On an ARMV6 build either counter may be live at
// runtime (the PMU value when user-mode access is enabled, otherwise the
// nanosecond clock), so we mask at the narrower 38-bit width. Nearly every
// elapsed value the scheduler compares against a threshold is far below 2^38 in
// its unit, so the mask returns the true value. The exception is the steal
// loop, which diffs against a baseline taken on entry and can span an
// arbitrarily long idle/suspend: when that exceeds the wrap period the value
// read is the true elapsed modulo the counter width. The worst case is a single
// transient, self-correcting timing decision (a delayed suspend, never an early
// one, never a correctness error) -- still far better than the raw subtraction
// this replaces, which misfired on every wrap.
uint64_t ponyint_cpu_tick_diff(uint64_t supposedly_earlier,
  uint64_t supposedly_later)
{
  return ponyint_cpu_tick_diff_bits(supposedly_earlier, supposedly_later,
    PONY_CPU_TICK_BITS);
}
