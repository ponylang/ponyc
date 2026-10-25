use "pony_bench"

actor Main is BenchmarkList
  new create(env: Env) =>
    PonyBench(env, this)

  fun tag benchmarks(bench: PonyBench) =>
    bench(_BoxU32)
    bench(_BoxU8)
    bench(_BoxBool)
    bench(_BoxF32)
    bench(_UnboxU32)
    bench(_UnboxF32)
    bench(_MatchU32OrNone)
    bench(_MatchThreeWay)
    bench(_IsBoxedU32)
    bench(_IsBoxedNone)
    bench(_IsntBoxedU32)
    bench(_ArrayStoreBoxed)
    bench(_ArrayLoadBoxed)
    bench(_MatchInLoop)
    bench(_BoxU64)
    bench(_UnboxU64)
    bench(_BoxF64)
    bench(_UnboxF64)
    bench(_MatchU64OrNone)
    bench(_IsBoxedU64)
    bench(_DispatchStringable)
