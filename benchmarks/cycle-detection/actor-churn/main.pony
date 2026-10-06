"""
Measures actor creation and destruction throughput.

A set of Churner actors continuously create short-lived Worker actors.
Each Worker receives a single message and then becomes garbage. This
exercises the actor lifecycle hot path: creation, scheduling, message
dispatch, blocking, and destruction (fast-reap or cycle-detector).

The benchmark runs for a fixed duration and reports per-interval and
aggregate actor throughput.
"""

use "cli"
use "collections"
use "time"

actor Main
  new create(env: Env) =>
    try
      let cs =
        CommandSpec.leaf(
          "actor-churn",
          "Measure actor creation/destruction throughput",
          [
            OptionSpec.i64(
              "churners",
              "Number of Churner actors"
              where default' = 4)
            OptionSpec.i64(
              "batch",
              "Workers created per Churner per round"
              where default' = 100)
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

      let num_churners = cmd.option("churners").i64().usize()
      let batch = cmd.option("batch").i64().usize()
      let duration = cmd.option("duration").i64().u64()

      env.out.print("# churners=" + num_churners.string()
        + " batch=" + batch.string()
        + " duration=" + duration.string() + "s")
      env.out.print("interval_ns,actors_created,actors_per_sec")

      let coordinator =
        Coordinator(env, num_churners, batch)
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
  let _churners: Array[Churner] val
  var _interval_count: U64 = 0
  var _total_count: U64 = 0
  var _waiting_for: USize = 0
  var _interval_start: U64
  var _bench_start: U64
  var _finishing: Bool = false

  new create(env: Env, num_churners: USize, batch: USize) =>
    _env = env
    let now = Time.nanos()
    _interval_start = now
    _bench_start = now

    let churners: Array[Churner] iso =
      recover Array[Churner](num_churners) end
    for i in Range(0, num_churners) do
      churners.push(Churner(this, batch))
    end
    _churners = consume churners

    for c in _churners.values() do
      c.churn()
    end

  be tick(done: Bool) =>
    _finishing = done
    for c in _churners.values() do
      c.stop(this)
    end
    _waiting_for = _churners.size()

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
          + _total_count.string() + " actors, "
          + total_rate.string() + " actors/s")
      else
        for c in _churners.values() do
          c.resume()
        end
      end
    end

actor Churner
  let _coordinator: Coordinator
  let _batch: USize
  var _go: Bool = true
  var _count: U64 = 0

  new create(coordinator: Coordinator, batch: USize) =>
    _coordinator = coordinator
    _batch = batch

  be churn() =>
    if _go then
      for _ in Range(0, _batch) do
        Worker.run()
        _count = _count + 1
      end
      churn()
    end

  be stop(coordinator: Coordinator) =>
    _go = false
    coordinator.report_stopped(_count)
    _count = 0

  be resume() =>
    _go = true
    churn()

actor Worker
  be run() =>
    None

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
