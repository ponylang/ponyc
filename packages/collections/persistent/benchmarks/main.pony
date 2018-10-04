use ".."
use mut = "collections"
use "ponybench"

type K is String
type V is U64

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    let size: USize = 32 * 32
    let ns = [as USize: 0; 4; 17; 21; 24; 30]

    for n in ns.values() do bench(MapApply(size, n)) end
    for n in ns.values() do bench(MapInsert(size, n)) end
    for n in ns.values() do bench(MapUpdate(size, n)) end
    bench(MapIter(32))
    bench(MapIter(32 * 32))
    bench(MapIter(32 * 32 * 32))

class iso MapApply is MicroBenchmark
  let _size: USize
  let _n: USize
  let _map: Map[K, V]

  new iso create(size: USize, n: USize) =>
    _size = size
    _n = n
    _map = GenMap(_size)

  fun name(): String =>
    " ".join(["apply"; _n; "size"; _size].values())

  fun apply() ? =>
    let x = _map(_n.string())?
    DoNotOptimise[V](x)
    DoNotOptimise.observe()

class iso MapInsert is MicroBenchmark
  let _size: USize
  let _n: USize
  let _map: Map[K, V]

  new iso create(size: USize, n: USize) =>
    _size = size
    _n = n
    _map = try GenMap(size).remove(n.string())? else GenMap(0) end

  fun name(): String =>
    " ".join(["insert"; _n; "size"; _size].values())

  fun apply() =>
    let m = _map(_n.string()) = _n.u64()
    DoNotOptimise[Map[K, V]](m)
    DoNotOptimise.observe()

class iso MapUpdate is MicroBenchmark
  let _size: USize
  let _n: USize
  let _map: Map[K, V]

  new iso create(size: USize, n: USize) =>
    _size = size
    _n = n
    _map = GenMap(_size)

  fun name(): String =>
    " ".join(["update"; _n; "size"; _size].values())

  fun apply() =>
    let m = _map.update(_n.string(), -_n.u64())
    DoNotOptimise[Map[K, V]](m)
    DoNotOptimise.observe()

class iso MapIter is MicroBenchmark
  let _size: USize
  let _map: Map[K, V]

  new iso create(size: USize) =>
    _size = size
    _map = GenMap(_size)

  fun name(): String =>
    " ".join(["iter size"; _size].values())

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
