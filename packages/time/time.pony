use "lib:rt" if linux

use @clock_gettime[I32](clock: U32, ts: Pointer[(I64, I64)])
  if lp64 and (linux or freebsd)

use @clock_gettime[I32](clock: U32, ts: Pointer[(I32, I32)])
  if ilp32 and (linux or freebsd)

use @mach_absolute_time[U64]() if osx

type _Clock is (_ClockRealtime | _ClockMonotonic)

primitive _ClockRealtime
  fun apply(): U32 => 0

primitive _ClockMonotonic
  fun apply(): U32 =>
    ifdef linux then
      1
    elseif freebsd then
      4
    else
      compile_error "no monotonic clock"
    end

primitive Time
  """
  A collection of ways to fetch the current time.
  """
  fun now(): (I64 /*sec*/, I64 /*nsec*/) =>
    """
    The wall-clock adjusted system time with nanoseconds.
    Return: (seconds, nanoseconds)
    """
    ifdef osx then
      var ts: (I64, I64) = (0, 0)
      @gettimeofday[I32](addressof ts, U64(0))
      (ts._1, ts._2 * 1000)
    elseif linux or freebsd then
      _clock_gettime(_ClockRealtime)
    elseif windows then
      var ft: (U32, U32) = (0, 0)
      @GetSystemTimeAsFileTime[None](addressof ft)
      var qft = ft._1.u64() or (ft._2.u64() << 32)
      var epoch = qft.i64() - 116444736000000000
      var sec = epoch / 10000000
      var nsec = (epoch - (sec * 10000000)) * 100
      (sec, nsec)
    else
      (0, 0)
    end

  fun seconds(): I64 =>
    """
    The wall-clock adjusted system time.
    """
    @time[I64](U64(0))

  fun millis(): U64 =>
    """
    Monotonic unadjusted milliseconds.
    """
    ifdef osx then
      @mach_absolute_time() / 1000000
    elseif linux or freebsd then
      var ts = _clock_gettime(_ClockMonotonic)
      ((ts._1 * 1000) + (ts._2 / 1000000)).u64()
    elseif windows then
      (let qpc, let qpf) = _query_performance_counter()
      (qpc * 1000) / qpf
    else
      0
    end

  fun micros(): U64 =>
    """
    Monotonic unadjusted microseconds.
    """
    ifdef osx then
      @mach_absolute_time() / 1000
    elseif linux or freebsd then
      var ts = _clock_gettime(_ClockMonotonic)
      ((ts._1 * 1000000) + (ts._2 / 1000)).u64()
    elseif windows then
      (let qpc, let qpf) = _query_performance_counter()
      (qpc * 1000000) / qpf
    else
      0
    end

  fun nanos(): U64 =>
    """
    Monotonic unadjusted nanoseconds.
    """
    ifdef osx then
      @mach_absolute_time()
    elseif linux or freebsd then
      var ts = _clock_gettime(_ClockMonotonic)
      ((ts._1 * 1000000000) + ts._2).u64()
    elseif windows then
      (let qpc, let qpf) = _query_performance_counter()
      (qpc * 1000000000) / qpf
    else
      0
    end

  fun wall_to_nanos(wall: (I64, I64)): U64 =>
    """
    Converts a wall-clock adjusted system time to monotonic unadjusted
    nanoseconds.
    """
    let wall_now = now()
    nanos() +
      (((wall._1 * 1000000000) + wall._2) -
      ((wall_now._1 * 1000000000) + wall_now._2)).u64()

  fun cycles(): U64 =>
    """
    Processor cycle count. Don't use this for performance timing, as it does
    not control for out-of-order execution.
    """
    @"llvm.readcyclecounter"[U64]()

  fun perf_begin(): U64 =>
    """
    Get a cycle count for beginning a performance testing block. This will
    will prevent instructions from before this call leaking into the block and
    instructions after this call being executed earlier.
    """
    ifdef x86 then
      @"internal.x86.cpuid"[(I32, I32, I32, I32)](I32(0))
      @"llvm.x86.rdtsc"[U64]()
    else
      0
    end

  fun perf_end(): U64 =>
    """
    Get a cycle count for ending a performance testing block. This will
    will prevent instructions from after this call leaking into the block and
    instructions before this call being executed later.
    """
    ifdef x86 then
      var aux: I32 = 0
      var ts = @"internal.x86.rdtscp"[U64](addressof aux)
      @"internal.x86.cpuid"[(I32, I32, I32, I32)](I32(0))
      ts
    else
      0
    end

  fun _clock_gettime(clock: _Clock): (I64, I64) =>
    """
    Return a clock time on linux and freebsd.
    """
    ifdef lp64 and (linux or freebsd) then
      var ts: (I64, I64) = (0, 0)
      @clock_gettime(clock(), addressof ts)
      ts
    elseif ilp32 and (linux or freebsd) then
      var ts: (I32, I32) = (0, 0)
      @clock_gettime(clock(), addressof ts)
      (ts._1.i64(), ts._2.i64())
    else
      (0, 0)
    end

  fun _query_performance_counter(): (U64 /* qpc */, U64 /* qpf */) =>
    """
    Return QPC and QPF.
    """
    ifdef windows then
      var pf: (U32, U32) = (0, 0)
      var pc: (U32, U32) = (0, 0)
      @QueryPerformanceFrequency[U32](addressof pf)
      @QueryPerformanceCounter[U32](addressof pc)
      let qpf = pf._1.u64() or (pf._2.u64() << 32)
      let qpc = pc._1.u64() or (pc._2.u64() << 32)
      (qpc, qpf)
    else
      (0, 0)
    end
