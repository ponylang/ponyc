use "ponybench"
use "random"

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    bench(RealHashingBench)
    bench(RealHashingSipHashBench)

class iso RealHashingBench is MicroBenchmark

  let random: Random = Rand
  var y: U64 = 0

  fun name(): String => "hashing/real"

  fun ref before_iteration() =>
    y = random.next()

  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[USize](y.hash())
    DoNotOptimise.observe()

class iso RealHashingSipHashBench is MicroBenchmark

  let random: Random = Rand
  var x: U64 = 0

  fun ref before_iteration() =>
    x = random.next()

  fun name(): String => "hashing/real/siphash"
  fun apply() =>
    // prevent compiler from optimizing out this operation
    DoNotOptimise[USize](siphash(x))
    DoNotOptimise.observe()

  fun siphash(hash_me: U64): USize =>
    Hashing.finish(Hashing.update(hash_me, Hashing.begin()))




