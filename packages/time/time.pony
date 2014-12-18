use "lib:rt" if linux

primitive Time
  """
  A collection of ways to fetch the current time.
  """

  fun tag seconds(): I64 =>
    """
    The wall-clock adjusted system time.
    """
    @time[I64](U64(0))

  fun tag now(): (I64, I64) =>
    """
    The wall-clock adjusted system time with nanoseconds.
    """
    if Platform.osx() then
      var ts: (I64, I64) = (0, 0)
      @gettimeofday[I32](&ts, U64(0))
      (ts._1, ts._2 * 1000)
    elseif Platform.linux() then
      var ts: (I64, I64) = (0, 0)
      @clock_gettime[I32](U64(0), &ts)
      ts
    elseif Platform.windows() then
      var ft: (U32, U32) = (0, 0)
      @GetSystemTimeAsFileTime[None](&ft)
      var qft = ft._1.u64() or (ft._2.u64() << 32)
      var epoch = qft.i64() - 116444736000000000
      var sec = epoch / 10000000
      var nsec = (epoch - (sec * 10000000)) * 100
      (sec, nsec)
    else
      (I64(0), I64(0))
    end

  fun tag nanos(): U64 =>
    """
    Monotonic unadjusted nanoseconds.
    """
    if Platform.osx() then
      @mach_absolute_time[U64]()
    elseif Platform.linux() then
      var ts: (U64, U64) = (0, 0)
      @clock_gettime[I32](U64(1), &ts)
      (ts._1 * 1000000000) + ts._2
    elseif Platform.windows() then
      var pf: (U32, U32) = (0, 0)
      var pc: (U32, U32) = (0, 0)
      @QueryPerformanceFrequency[U32](&pf)
      @QueryPerformanceCounter[U32](&pc)
      var qpf = pf._1.u64() or (pf._2.u64() << 32)
      var qpc = pc._1.u64() or (pc._2.u64() << 32)
      (qpc * 1000000000) / qpf
    else
      0
    end

  fun tag cycles(): U64 =>
    """
    Processor cycle count. Don't use this for performance timing, as it does
    not control for out-of-order execution.
    """
    @"llvm.readcyclecounter"[U64]()

  fun tag perf_begin(): U64 =>
    """
    Get a cycle count for beginning a performance testing block. This will
    will prevent instructions from before this call leaking into the block and
    instructions after this call being executed earlier.
    """
    @"internal.x86.cpuid"[(I32, I32, I32, I32)](I32(0))
    @"llvm.x86.rdtsc"[U64]()

  fun tag perf_end(): U64 =>
    """
    Get a cycle count for ending a performance testing block. This will
    will prevent instructions from after this call leaking into the block and
    instructions before this call being executed later.
    """
    var aux: I32 = 0
    var ts = @"internal.x86.rdtscp"[U64](&aux)
    @"internal.x86.cpuid"[(I32, I32, I32, I32)](I32(0))
    ts
