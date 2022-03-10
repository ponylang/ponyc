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

primitive Even is Stringable
   fun string(): String iso^ => "even".clone()
primitive Skew is Stringable
   fun string(): String iso^ => "skew".clone()

// An "enumeration" type
type Dist is (Even | Skew)

primitive Distribution
  fun even(n: USize): List[U64] =>
    var list: List[U64] iso = recover iso List[U64](n) end
    var remaining = n
    let rnd = Randomness
    while remaining != 0 do
      list.push(rnd.u64())
      remaining = remaining - 1
    end
    consume list

  fun skew(n: USize): List[U64] =>
    var list: List[U64] iso = recover iso List[U64](n) end
    var remaining = n
    let rnd = Randomness
    let max: U64 = U64.max_value() / 2
    while remaining != 0 do
      list.push(rnd.u64(0, max))
      remaining = remaining - 1
    end
    consume list

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    // bench(_Map(100, Even))
    // bench(_Map(10_000, Even))
    // bench(_Map(1_000_000, Even))
    // bench(_Map(100, Skew))
    // bench(_Map(10_000, Skew))
    // bench(_Map(1_000_000, Skew))

    // bench(_Filter(100, Even))
    // bench(_Filter(10_000, Even))
    // bench(_Filter(500_000, Even)) // Reduced size due to stack overflow from missing TCO
    // bench(_Filter(100, Skew))
    // bench(_Filter(10_000, Skew))
    // bench(_Filter(500_000, Skew)) // Reduced size due to stack overflow from missing TCO

    // bench(_Fold(100, Even))
    // bench(_Fold(10_000, Even))
    // bench(_Fold(1_000_000, Even))
    // bench(_Fold(100, Skew))
    // bench(_Fold(10_000, Skew))
    // bench(_Fold(1_000_000, Skew))

    bench(_Every(100, Even))
    bench(_Every(10_000, Even))
    bench(_Every(1_000_000, Even))
    bench(_Every(100, Skew))
    bench(_Every(10_000, Skew))
    bench(_Every(1_000_000, Skew))

class iso _Map is MicroBenchmark
  let _n: USize
  var _list: List[U64]
  let _dist: Dist

  new iso create(n: USize, dist: Dist) =>
    _n = n
    _list = List[U64](n)
    _dist = dist


  fun ref before() =>
    _list = match _dist
      | Even => Distribution.even(_n)
      | Skew => Distribution.skew(_n)
    end

  fun ref after() =>
    _list.clear()

  fun name(): String => "dist: " + _dist.string() + ", size: " + _n.string() + " func: map +1 "

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[List[U64]](_list.map[U64]( {(v: U64): U64 => v + 1 } ))
    DoNotOptimise.observe()

class iso _Filter is MicroBenchmark
  let _n: USize
  var _list: List[U64]
  let _dist: Dist

  new iso create(n: USize, dist: Dist) =>
    _n = n
    _list = List[U64](n)
    _dist = dist


  fun ref before() =>
    _list = match _dist
      | Even => Distribution.even(_n)
      | Skew => Distribution.skew(_n)
    end

  fun ref after() =>
    _list.clear()

  fun name(): String => "dist: " + _dist.string() + ", size: " + _n.string() + " func: filter v % 2 == 0 "

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[List[U64]](_list.filter( {(v: U64): Bool => (v % 2) == 0 } ))
    DoNotOptimise.observe()

class iso _Fold is MicroBenchmark
  let _n: USize
  var _list: List[U64]
  let _dist: Dist

  new iso create(n: USize, dist: Dist) =>
    _n = n
    _list = List[U64](n)
    _dist = dist


  fun ref before() =>
    _list = match _dist
      | Even => Distribution.even(_n)
      | Skew => Distribution.skew(_n)
    end

  fun ref after() =>
    _list.clear()

  fun name(): String => "dist: " + _dist.string() + ", size: " + _n.string() + " func: fold max "

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[U64](_list.fold[U64]( {(acc: U64, v: U64): U64 => if acc > v then acc else v end }, 0 ))
    DoNotOptimise.observe()

class iso _Every is MicroBenchmark
  let _n: USize
  var _list: List[U64]
  let _dist: Dist

  new iso create(n: USize, dist: Dist) =>
    _n = n
    _list = List[U64](n)
    _dist = dist


  fun ref before() =>
    _list = match _dist
      | Even => Distribution.even(_n)
      | Skew => Distribution.skew(_n)
    end

  fun ref after() =>
    _list.clear()

  fun name(): String => "dist: " + _dist.string() + ", size: " + _n.string() + " func: every <1/2 max "

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[Bool](_list.every( {(v: U64): Bool => v < (U64.max_value() / 2) }))
    DoNotOptimise.observe()
