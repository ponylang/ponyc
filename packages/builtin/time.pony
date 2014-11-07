use "lib:rt" where linux

primitive Time
  // Wall-clock adjusted system time.
  fun tag seconds(): I64 => @time[I64](U64(0))

  // Wall-clock adjusted system time with nanoseconds.
  fun tag now(): (I64, I64) =>
    if Platform.osx() then
      var ts: (I64, I64) = (0, 0)
      @gettimeofday[I32](ts, U64(0))
      (ts._1, ts._2 * 1000)
    elseif Platform.linux() then
      var ts: (I64, I64) = (0, 0)
      @clock_gettime[I32](U64(0), ts)
      ts
    elseif Platform.windows() then
      var ft: (U32, U32) = (0, 0)
      @GetSystemTimeAsFileTime[None](ft)
      var qft = ft._1.u64() or (ft._2.u64() << 32)
      var epoch = qft.i64() - 116444736000000000
      var sec = epoch / 10000000
      var nsec = (epoch - (sec * 10000000)) * 100
      (sec, nsec)
    else
      (I64(0), I64(0))
    end

  // Monotonic unadjusted nanoseconds.
  fun tag nanos(): U64 =>
    if Platform.osx() then
      @mach_absolute_time[U64]()
    elseif Platform.linux() then
      var ts: (U64, U64) = (0, 0)
      @clock_gettime[I32](U64(1), ts)
      (ts._1 * 1000000000) + ts._2
    elseif Platform.windows() then
      var pf: (U32, U32) = (0, 0)
      var pc: (U32, U32) = (0, 0)
      @QueryPerformanceFrequency[U32](pf)
      @QueryPerformanceCounter[U32](pc)
      var qpf = pf._1.u64() or (pf._2.u64() << 32)
      var qpc = pc._1.u64() or (pc._2.u64() << 32)
      (qpc * 1000000000) / qpf
    else
      U64(0)
    end

  // Processor cycle count.
  fun tag cycles(): U64 => @llvm.readcyclecounter[U64]()
