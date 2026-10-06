"""
Measures cyclic actor creation and collection throughput.

Spawner actors continuously create small rings of actors that pass a few
messages then block. Each ring forms a genuine reference cycle requiring
the cycle detector to collect. A timer coordinates periodic throughput
reporting.
"""

use "cli"
use "collections"
use "time"

actor Main
  new create(env: Env) =>
    try
      let cs =
        CommandSpec.leaf(
          "cyclic-churn",
          "Measure cyclic actor creation/collection throughput",
          [
            OptionSpec.i64(
              "spawners",
              "Number of Spawner actors"
              where default' = 4)
            OptionSpec.i64(
              "ring-size",
              "Actors per ring"
              where default' = 5)
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
      let ring_size = cmd.option("ring-size").i64().usize()
      let passes = cmd.option("passes").i64().usize()
      let duration = cmd.option("duration").i64().u64()

      env.out.print("# spawners=" + num_spawners.string()
        + " ring-size=" + ring_size.string()
        + " passes=" + passes.string()
        + " duration=" + duration.string() + "s")
      env.out.print("interval_ns,rings_created,rings_per_sec")

      let coordinator = Coordinator(env, num_spawners, ring_size, passes)
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
      spawners.push(Spawner(this, ring_size, passes))
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
          + _total_count.string() + " rings, "
          + total_rate.string() + " rings/s")
      else
        for s in _spawners.values() do
          s.resume()
        end
      end
    end

actor Spawner
  let _coordinator: Coordinator
  let _ring_size: USize
  let _passes: USize
  var _go: Bool = true
  var _count: U64 = 0

  new create(coordinator: Coordinator, ring_size: USize, passes: USize) =>
    _coordinator = coordinator
    _ring_size = ring_size
    _passes = passes

  be churn() =>
    if _go then
      _create_ring()
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

  fun ref _create_ring() =>
    let first = RingActor
    var prev = first
    for i in Range(1, _ring_size) do
      let next = RingActor
      prev.set_next(next)
      prev = next
    end
    prev.set_next(first)
    first.pass(_passes.u64())

actor RingActor
  var _next: (RingActor | None) = None

  new create() =>
    None

  be set_next(next: RingActor) =>
    _next = next

  be pass(n: U64) =>
    if n > 0 then
      match _next
      | let next: RingActor => next.pass(n - 1)
      end
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
