"""
Measures actor message-passing throughput.

A set of Pinger actors forwards ping messages to randomly chosen peers.
A coordinator periodically collects counts, then restarts the cycle.
The benchmark runs for a fixed duration and reports per-interval and
aggregate message rates.

The number of in-flight messages is bounded by the initial-pings parameter
times the number of pingers, keeping memory consumption predictable.
"""

use "cli"
use "collections"
use "random"
use "time"
use @ponyint_cpu_tick[U64]()

actor Main
  new create(env: Env) =>
    try
      let cs =
        CommandSpec.leaf(
          "message-ubench",
          "Measure actor message-passing throughput",
          [
            OptionSpec.i64(
              "pingers",
              "Number of Pinger actors"
              where default' = 8)
            OptionSpec.i64(
              "initial-pings",
              "Initial pings per Pinger per interval"
              where default' = 5)
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

      let num_pingers = cmd.option("pingers").i64().usize()
      let initial_pings = cmd.option("initial-pings").i64().usize()
      let duration = cmd.option("duration").i64().u64()

      env.out.print("# pingers=" + num_pingers.string()
        + " initial-pings=" + initial_pings.string()
        + " duration=" + duration.string() + "s")
      env.out.print("interval_ns,messages,messages_per_sec")

      let coordinator =
        Coordinator(env, num_pingers, initial_pings)
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
  let _pingers: Array[Pinger] val
  let _initial_pings: USize
  var _interval_count: U64 = 0
  var _total_count: U64 = 0
  var _waiting_for: USize = 0
  var _interval_start: U64
  var _bench_start: U64
  var _finishing: Bool = false

  new create(env: Env, num_pingers: USize, initial_pings: USize) =>
    _env = env
    _initial_pings = initial_pings
    let now = Time.nanos()
    _interval_start = now
    _bench_start = now

    let pingers: Array[Pinger] iso = recover Array[Pinger](num_pingers) end
    for i in Range(0, num_pingers) do
      pingers.push(Pinger(i.u32()))
    end
    let pingers': Array[Pinger] val = consume pingers
    for p in pingers'.values() do
      p.set_neighbors(pingers')
    end
    _pingers = pingers'
    _start_next_round()

  be _start_next_round() =>
    for p in _pingers.values() do
      p.go()
    end
    for _ in Range(0, _initial_pings) do
      for p in _pingers.values() do
        p.ping(42)
      end
    end

  be tick(done: Bool) =>
    _finishing = done
    for p in _pingers.values() do
      p.stop(this)
    end
    _waiting_for = _pingers.size()

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
          + _total_count.string() + " messages, "
          + total_rate.string() + " msg/s")
      else
        _start_next_round()
      end
    end

actor Pinger
  let _id: U32
  var _neighbors: Array[Pinger] val = recover Array[Pinger] end
  var _num_neighbors: U64 = 0
  var _go: Bool = false
  var _count: U64 = 0
  let _rand: Rand

  new create(id: U32) =>
    _id = id
    let tsc: U64 = @ponyint_cpu_tick()
    (_, let t2: I64) = Time.now()
    _rand = Rand(tsc, t2.u64())
    _rand.int(100); _rand.int(100); _rand.int(100)

  be set_neighbors(ps: Array[Pinger] val) =>
    _neighbors = ps
    _num_neighbors = ps.size().u64()

  be go() =>
    _go = true
    _count = 0

  be stop(coordinator: Coordinator) =>
    _go = false
    coordinator.report_stopped(_count)

  be ping(payload: U64) =>
    if _go then
      _count = _count + 1
      try
        _neighbors(_rand.int(_num_neighbors).usize())?.ping(payload)
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
