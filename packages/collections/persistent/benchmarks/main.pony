use ".."
use mut = "collections"
use "ponybench"

type K is String
type V is U64

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    for n in mut.Range(0, 14, 2) do
      bench(MapApply(32 << n))
    end
    for n in mut.Range(0, 14, 2) do
      bench(MapUpdate(32 << n))
    end
    for n in mut.Range(0, 14, 2) do
      bench(MapIter(32 << n))
    end

class iso MapUpdate is MicroBenchmark
  let _size: USize
  let _map: Map[K, V]

  new iso create(size: USize) =>
    _size = size
    _map = GenMap(_size)

  fun name(): String =>
    " ".join(["update size"; _size].values())

  fun apply() =>
    let m = _map.update(_size.string(), _size.u64())
    DoNotOptimise[Map[K, V]](m)
    DoNotOptimise.observe()

class iso MapApply is MicroBenchmark
  let _size: USize
  let _map: Map[K, V]

  new iso create(size: USize) =>
    _size = size
    _map = GenMap(_size)

  fun name(): String =>
    " ".join(["apply size"; _size].values())

  fun apply() ? =>
    let x = _map("0")?
    DoNotOptimise[V](x)
    DoNotOptimise.observe()

class iso MapIter is MicroBenchmark
  let _size: USize
  let _map: Map[K, V]

  new iso create(size: USize) =>
    _size = size
    _map = GenMap(_size)

  fun name(): String =>
    " ".join(["apply size"; _size].values())

  fun apply() ? =>
    for i in mut.Range(0, _size) do
      let x = _map(i.string())?
      DoNotOptimise[V](x)
    end
    DoNotOptimise.observe()

primitive GenMap
  fun apply(size: USize): Map[K, V] =>
    var m = Map[K, V]
    for i in mut.Range(0, size) do
      m = m(i.string()) = i.u64()
    end
    m
