"""
Measures overlapping cyclic actor creation and collection throughput.

Like cyclic-churn but creates clusters of overlapping rings that share
actors at their boundaries. This exercises the cycle detector's component
merging path where connected components must be merged before
confirmation.

Each cluster is a chain of overlapping rings. Consecutive rings share one
actor: the last unique actor of ring N is also a member of ring N+1.
The shared actor holds references into both rings, connecting the cycles
into one component.
"""

use "cli"
use "collections"
use "time"

actor Main
  new create(env: Env) =>
    try
      let cs =
        CommandSpec.leaf(
          "overlapping-cyclic-churn",
          "Measure overlapping cyclic actor creation/collection throughput",
          [
            OptionSpec.i64(
              "spawners",
              "Number of Spawner actors"
              where default' = 4)
            OptionSpec.i64(
              "rings-per-cluster",
              "Number of overlapping rings per cluster"
              where default' = 3)
            OptionSpec.i64(
              "ring-size",
              "Actors per ring including shared actor"
              where default' = 4)
            OptionSpec.i64(
              "passes",
              "Messages around each ring before it blocks"
              where default' = 10)
            OptionSpec.i64(
              "duration",
              "Total benchmark duration in seconds"
              where default' = 60)
          ])? .> add_help()?
      let cmd =
        match \exhaustive\ CommandParser(cs).parse(env.args, env.vars)
        | let c: Command => c
        | let ch: CommandHelp =>
          ch.print_help(env.out)
          env.exitcode(0)
          error
        | let se: SyntaxError =>
          env.out.print(se.string())
          env.exitcode(1)
          error
        end

      let num_spawners = cmd.option("spawners").i64().usize()
      let rings_per_cluster = cmd.option("rings-per-cluster").i64().usize()
      let ring_size = cmd.option("ring-size").i64().usize()
      let passes = cmd.option("passes").i64().usize()
      let duration = cmd.option("duration").i64().u64()

      env.out.print("# spawners=" + num_spawners.string()
        + " rings-per-cluster=" + rings_per_cluster.string()
        + " ring-size=" + ring_size.string()
        + " passes=" + passes.string()
        + " duration=" + duration.string() + "s")
      env.out.print("interval_ns,clusters_created,clusters_per_sec")

      let coordinator =
        Coordinator(env, num_spawners, rings_per_cluster, ring_size, passes)
      let interval_ns: U64 = 1_000_000_000
      let timers = Timers
      let timer =
        Timer(
          TickNotify(coordinator, duration.usize()),
          interval_ns, interval_ns)
      timers(consume timer)
    else
      env.exitcode(1)
    end

actor Coordinator
  let _env: Env
  let _spawners: Array[Spawner] val
  var _interval_count: U64 = 0
  var _total_count: U64 = 0
  var _waiting_for: USize = 0
  var _interval_start: U64
  var _bench_start: U64
  var _finishing: Bool = false

  new create(
    env: Env,
    num_spawners: USize,
    rings_per_cluster: USize,
    ring_size: USize,
    passes: USize)
  =>
    _env = env
    let now = Time.nanos()
    _interval_start = now
    _bench_start = now

    let spawners: Array[Spawner] iso =
      recover Array[Spawner](num_spawners) end
    for i in Range(0, num_spawners) do
      spawners.push(Spawner(this, rings_per_cluster, ring_size, passes))
    end
    _spawners = consume spawners

    for s in _spawners.values() do
      s.churn()
    end

  be tick(done: Bool) =>
    _finishing = done
    for s in _spawners.values() do
      s.stop(this)
    end
    _waiting_for = _spawners.size()

  be report_stopped(count: U64) =>
    _interval_count = _interval_count + count
    _waiting_for = _waiting_for - 1
    if _waiting_for == 0 then
      let now = Time.nanos()
      let interval_ns = now - _interval_start
      _total_count = _total_count + _interval_count
      let rate =
        if interval_ns > 0 then
          (_interval_count * 1_000_000_000) / interval_ns
        else
          U64(0)
        end
      _env.out.print(interval_ns.string() + ","
        + _interval_count.string() + "," + rate.string())
      _interval_count = 0
      _interval_start = now

      if _finishing then
        let total_ns = now - _bench_start
        let total_rate =
          if total_ns > 0 then
            (_total_count * 1_000_000_000) / total_ns
          else
            U64(0)
          end
        _env.out.print("# total: " + total_ns.string() + "ns, "
          + _total_count.string() + " clusters, "
          + total_rate.string() + " clusters/s")
      else
        for s in _spawners.values() do
          s.resume()
        end
      end
    end

actor Spawner
  let _coordinator: Coordinator
  let _rings_per_cluster: USize
  let _ring_size: USize
  let _passes: USize
  var _go: Bool = true
  var _count: U64 = 0

  new create(
    coordinator: Coordinator,
    rings_per_cluster: USize,
    ring_size: USize,
    passes: USize)
  =>
    _coordinator = coordinator
    _rings_per_cluster = rings_per_cluster
    _ring_size = ring_size
    _passes = passes

  be churn() =>
    if _go then
      _create_cluster()
      _count = _count + 1
      churn()
    end

  be stop(coordinator: Coordinator) =>
    _go = false
    coordinator.report_stopped(_count)
    _count = 0

  be resume() =>
    _go = true
    churn()

  fun ref _create_cluster() =>
    """
    Build a chain of overlapping rings. Each ring has `_ring_size` actors.
    Consecutive rings share one actor: the last unique actor of ring N
    becomes a member of ring N+1. The shared actor holds references into
    both rings via its neighbors list.

    Example with ring-size=4, rings-per-cluster=3:
      Ring 1: A -> B -> C -> A  (shared actor: C)
      Ring 2: C -> D -> E -> C  (shared actor: E)
      Ring 3: E -> F -> G -> E
    Actor C is in ring 1 (via A->B->C) and ring 2 (C->D->E->C).
    """
    var shared = RingActor
    for ring_idx in Range(0, _rings_per_cluster) do
      let first = shared
      var prev = first
      let unique_count = _ring_size - 1
      for i in Range(0, unique_count) do
        let next = RingActor
        prev.add_next(next)
        prev = next
      end
      prev.add_next(first)
      first.pass(_passes.u64())
      if ring_idx < (_rings_per_cluster - 1) then
        shared = prev
      end
    end

actor RingActor
  let _nexts: Array[RingActor]

  new create() =>
    _nexts = Array[RingActor](2)

  be add_next(next: RingActor) =>
    _nexts.push(next)

  be pass(n: U64) =>
    if n > 0 then
      try _nexts(0)?.pass(n - 1) end
    end

class TickNotify is TimerNotify
  let _coordinator: Coordinator
  var _remaining: USize

  new iso create(coordinator: Coordinator, total_secs: USize) =>
    _coordinator = coordinator
    _remaining = total_secs

  fun ref apply(timer: Timer, count: U64): Bool =>
    _remaining = _remaining - count.usize().min(_remaining)
    let done = _remaining == 0
    _coordinator.tick(done)
    not done
