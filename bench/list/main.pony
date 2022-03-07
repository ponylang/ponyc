use "collections"
use "pony_bench"
use "pony_check"

/*
Plan: Benchmark each reimplemented method on List across a 3x2 matrix of inputs representing
3 expected usage sizes (100, 10k, 1m) using 2 data distributions (uniform and skewed).

The reimplemented methods from List are as follows:
+ map
+ filter
+ fold
+ every
+ exists
+ drop
+ take
+ take_while
*/

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    bench(_EvenDistributionMap(100))
    bench(_EvenDistributionMap(10_000))
    bench(_EvenDistributionMap(1_000_000))
    bench(_SkewDistributionMap(100))
    bench(_SkewDistributionMap(10_000))
    bench(_SkewDistributionMap(1_000_000))

class iso _EvenDistributionMap is MicroBenchmark
  var _list: List[U64]
  let _n: U64

  new iso create(n: U64) =>
    _list = List[U64]
    _n = n

  fun ref before() =>
    var remaining = _n
    let rnd = Randomness
    while remaining != 0 do
      _list.push(rnd.u64())
      remaining = remaining - 1
    end

  fun ref after() =>
    _list.clear()

  fun name(): String => "dist: even, size: " + _n.string() + " func: map +1 "

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[List[U64]](_list.map[U64]( {(v: U64): U64 => v + 1 } ))
    DoNotOptimise.observe()

class iso _SkewDistributionMap is MicroBenchmark
  var _list: List[U64]
  let _n: U64

  new iso create(n: U64) =>
    _list = List[U64]
    _n = n

  fun ref before() =>
    var remaining = _n
    let rnd = Randomness
    let max: U64 = U64.max_value() / 2
    while remaining != 0 do
      _list.push(rnd.u64(0, max))
      remaining = remaining - 1
    end

  fun ref after() =>
    _list.clear()

  fun name(): String => "dist: skew, size: " + _n.string() + " func: map +1 "

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[List[U64]](_list.map[U64]( {(v: U64): U64 => v + 1 } ))
    DoNotOptimise.observe()