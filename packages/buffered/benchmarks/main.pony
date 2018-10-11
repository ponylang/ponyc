use ".."
use "ponybench"
use "collections"

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    bench(_ReaderU8)
    bench(_ReaderU16LE)
    bench(_ReaderU16LESplit)
    bench(_ReaderU16BE)
    bench(_ReaderU16BESplit)
    bench(_ReaderU32LE)
    bench(_ReaderU32LESplit)
    bench(_ReaderU32BE)
    bench(_ReaderU32BESplit)
    bench(_ReaderU64LE)
    bench(_ReaderU64LESplit)
    bench(_ReaderU64BE)
    bench(_ReaderU64BESplit)
    bench(_ReaderU128LE)
    bench(_ReaderU128LESplit)
    bench(_ReaderU128BE)
    bench(_ReaderU128BESplit)
    bench(_WriterU8)
    bench(_WriterU16LE)
    bench(_WriterU16BE)
    bench(_WriterU32LE)
    bench(_WriterU32BE)
    bench(_WriterU64LE)
    bench(_WriterU64BE)
    bench(_WriterU128LE)
    bench(_WriterU128BE)

class iso _ReaderU8 is MicroBenchmark
  // Benchmark reading U8
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU8"

  fun ref apply()? =>
    DoNotOptimise[U8](_d.u8()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU16LE is MicroBenchmark
  // Benchmark reading Little Endian U16
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU16LE"

  fun ref apply()? =>
    DoNotOptimise[U16](_d.u16_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU16LESplit is MicroBenchmark
  // Benchmark reading split Little Endian U16
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU16LESplit"

  fun ref apply()? =>
    DoNotOptimise[U16](_d.u16_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _ReaderU16BE is MicroBenchmark
  // Benchmark reading Big Endian U16
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU16BE"

  fun ref apply()? =>
    DoNotOptimise[U16](_d.u16_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU16BESplit is MicroBenchmark
  // Benchmark reading split Big Endian U16
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU16BESplit"

  fun ref apply()? =>
    DoNotOptimise[U16](_d.u16_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _ReaderU32LE is MicroBenchmark
  // Benchmark reading Little Endian U32
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU32LE"

  fun ref apply()? =>
    DoNotOptimise[U32](_d.u32_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU32LESplit is MicroBenchmark
  // Benchmark reading split Little Endian U32
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU32LESplit"

  fun ref apply()? =>
    DoNotOptimise[U32](_d.u32_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _ReaderU32BE is MicroBenchmark
  // Benchmark reading Big Endian U32
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU32BE"

  fun ref apply()? =>
    DoNotOptimise[U32](_d.u32_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU32BESplit is MicroBenchmark
  // Benchmark reading split Big Endian U32
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU32BESplit"

  fun ref apply()? =>
    DoNotOptimise[U32](_d.u32_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _ReaderU64LE is MicroBenchmark
  // Benchmark reading Little Endian U64
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU64LE"

  fun ref apply()? =>
    DoNotOptimise[U64](_d.u64_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU64LESplit is MicroBenchmark
  // Benchmark reading split Little Endian U64
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU64LESplit"

  fun ref apply()? =>
    DoNotOptimise[U64](_d.u64_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _ReaderU64BE is MicroBenchmark
  // Benchmark reading Big Endian U64
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU64BE"

  fun ref apply()? =>
    DoNotOptimise[U64](_d.u64_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU64BESplit is MicroBenchmark
  // Benchmark reading split Big Endian U64
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU64BESplit"

  fun ref apply()? =>
    DoNotOptimise[U64](_d.u64_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _ReaderU128LE is MicroBenchmark
  // Benchmark reading Little Endian U128
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU128LE"

  fun ref apply()? =>
    DoNotOptimise[U128](_d.u128_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU128LESplit is MicroBenchmark
  // Benchmark reading split Little Endian U128
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU128LESplit"

  fun ref apply()? =>
    DoNotOptimise[U128](_d.u128_le()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _ReaderU128BE is MicroBenchmark
  // Benchmark reading Big Endian U128
  let _d: Reader = _d.create()

  new iso create() =>
    _d.append(recover Array[U8].>undefined(10485760) end)

  fun name(): String =>
    "_ReaderU128BE"

  fun ref apply()? =>
    DoNotOptimise[U128](_d.u128_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      _d.append(recover Array[U8].>undefined(10485760) end)
    end

class iso _ReaderU128BESplit is MicroBenchmark
  // Benchmark reading split Big Endian U128
  let _d: Reader = _d.create()
  let _a: Array[Array[U8] val] = _a.create()

  new iso create() =>
    while _a.size() < 65536 do
      _a.push(recover Array[U8].>undefined(1) end)
    end

    for b in _a.values() do
      _d.append(b)
    end

  fun name(): String =>
    "_ReaderU128BESplit"

  fun ref apply()? =>
    DoNotOptimise[U128](_d.u128_be()?)
    DoNotOptimise.observe()
    if _d.size() < 128 then
      _d.clear()
      for b in _a.values() do
        _d.append(b)
      end
    end

class iso _WriterU8 is MicroBenchmark
  // Benchmark writing U8
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U8 = 0

  fun name(): String =>
    "_WriterU8"

  fun ref apply() =>
    DoNotOptimise[None](_d.u8(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU16LE is MicroBenchmark
  // Benchmark writing Little Endian U16
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U16 = 0

  fun name(): String =>
    "_WriterU16LE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u16_le(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU16BE is MicroBenchmark
  // Benchmark writing Big Endian U16
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U16 = 0

  fun name(): String =>
    "_WriterU16BE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u16_be(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU32LE is MicroBenchmark
  // Benchmark writing Little Endian U32
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U32 = 0

  fun name(): String =>
    "_WriterU32LE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u32_le(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU32BE is MicroBenchmark
  // Benchmark writing Big Endian U32
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U32 = 0

  fun name(): String =>
    "_WriterU32BE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u32_be(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU64LE is MicroBenchmark
  // Benchmark writing Little Endian U64
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U64 = 0

  fun name(): String =>
    "_WriterU64LE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u64_le(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU64BE is MicroBenchmark
  // Benchmark writing Big Endian U64
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U64 = 0

  fun name(): String =>
    "_WriterU64BE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u64_be(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU128LE is MicroBenchmark
  // Benchmark writing Little Endian U128
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U128 = 0

  fun name(): String =>
    "_WriterU128LE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u128_le(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end

class iso _WriterU128BE is MicroBenchmark
  // Benchmark writing Big Endian U128
  let _d: Writer = _d.create() .> reserve_current(10485760)
  var _i: U128 = 0

  fun name(): String =>
    "_WriterU128BE"

  fun ref apply() =>
    DoNotOptimise[None](_d.u128_be(_i))
    DoNotOptimise.observe()
    _i = _i + 1
    if _d.size() > 10485760 then
      _d.done()
      _d.reserve_current(10485760)
    end
