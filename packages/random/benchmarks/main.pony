use ".."
use "ponybench"

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    bench(RandIntBenchmark)
    bench(RandIntUnbiasedBenchmark)
    bench(RandIntFPMultBenchmark)
    bench(RandomBenchmark[MT]("mt"))
    bench(RandomBenchmark[XorShift128Plus]("xorshift128+"))
    bench(RandomBenchmark[XorOshiro128Plus]("xoroshiro128+"))
    bench(RandomBenchmark[XorOshiro128StarStar]("xoroshiro128**"))
    bench(RandomBenchmark[SplitMix64]("splitmix64"))

class iso RandIntBenchmark is MicroBenchmark
  let _rand: Rand
  var _bound: U64 = 0

  new iso create() =>
    _rand = Rand.create()

  fun name(): String => "random/int"

  fun ref before_iteration() =>
    _bound = _rand.next()

  fun ref apply() =>
    let x = _rand.int(_bound)
    DoNotOptimise[U64](x)
    DoNotOptimise.observe()

class iso RandIntUnbiasedBenchmark is MicroBenchmark
  let _rand: Rand
  var _bound: U64 = 0

  new iso create() =>
    _rand = Rand.create()

  fun name(): String => "random/int-unbiased"

  fun ref before_iteration() =>
    _bound = _rand.next()

  fun ref apply() =>
    let x = _rand.int_unbiased(_bound)
    DoNotOptimise[U64](x)
    DoNotOptimise.observe()

class iso RandIntFPMultBenchmark is MicroBenchmark
  let _rand: Rand
  var _bound: U64 = 0

  new iso create() =>
    _rand = Rand.create()

  fun name(): String => "random/int-fp-mult"

  fun ref before_iteration() =>
    _bound = _rand.next()

  fun ref apply() =>
    let x = _rand.int_fp_mult(_bound)
    DoNotOptimise[U64](x)
    DoNotOptimise.observe()

class iso RandomBenchmark[T: Random ref] is MicroBenchmark
  let _rand: T
  let _name: String

  new iso create(name': String) =>
    _name = name'
    _rand = T.create()

  fun name(): String => "random/next/" + _name

  fun ref apply() =>
    let x: U64 = _rand.next()
    DoNotOptimise[U64](x)
    DoNotOptimise.observe()
