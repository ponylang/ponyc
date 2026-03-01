"""
A microbenchmark for measuring message passing rates in the Pony runtime.

This microbenchmark executes a sequence of intervals.  During an interval,
1 second long by default, the SyncLeader actor sends an initial
set of ping messages to a static set of Pinger actors.  When a Pinger
actor receives a ping() message, the Pinger will randomly choose
another Pinger to forward the ping() message.  This technique limits
the total number of messages "in flight" in the runtime to avoid
causing unnecessary memory consumption & overhead by the Pony runtime.

This small program has several intended uses:

* Demonstrate use of three types of actors in a Pony program: a timer,
  a SyncLeader, and many Pinger actors.

* As a stress test for Pony runtime development, for example, finding
  deadlocks caused by experiments in the "Generalized runtime
  backpressure" work in pull request
  https://github.com/ponylang/ponyc/pull/2264

* As a stress test for measuring message send & receive overhead for
  experiments in the "Add DTrace probes for all message push and pop
  operations" work in pull request
  https://github.com/ponylang/ponyc/pull/2295
"""

use "assert"
use "cli"
use "collections"
use "random"
use "time"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @ponyint_cpu_tick[U64]()

actor Main
  new create(env: Env) =>
    """
    Parse the command line arguments, then create a SyncLeader actor
    and an interval timer that will coordinate all further computation.
    """
    try
      let cs =
        CommandSpec.leaf("do",
          "A message-passing micro-benchmark for the Pony runtime",
          [
            OptionSpec.i64("pingers",
              "Number of intra-process Pony ping actors"
              where default' = 8)
            OptionSpec.i64("report-interval",
              "Print report every N centiseconds (10 centiseconds = 1 second)"
              where default' = 10)
            OptionSpec.i64("report-count",
              "Number of reports to generate, default 0 is infinite"
              where default' = 0)
            OptionSpec.i64("initial-pings",
              "Initial # of pings to send to each Pinger actor in an interval"
              where default' = 5)
          ],
          [
            ArgSpec.string_seq("", "")
          ])?.>add_help()?
      let cmd =
      match \exhaustive\ CommandParser(cs).parse(env.args, env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
        ch.print_help(env.out)
        error
      | let se: SyntaxError =>
        env.out.print(se.string())
        error
      end

      let num_pingers = cmd.option("pingers").i64()
      let report_interval = cmd.option("report-interval").i64()
      let report_count = cmd.option("report-count").i64().u64()
      let initial_pings = cmd.option("initial-pings").i64()

      env.out.print("# " +
        "pingers " + num_pingers.string() + ", " +
        "report-interval " + report_interval.string() + ", " +
        "report-count " + report_count.string() + ", " +
        "initial-pings " + initial_pings.string())
      env.out.print("time,run-ns,rate")

      let sync_leader = SyncLeader(env,
        num_pingers.i32(), initial_pings.usize())
      let interval: U64 = (report_interval.u64() * 1_000_000_000) / 10
      let timers = Timers
      let timer = Timer(Tick(env, sync_leader, report_count), interval, interval)
      timers(consume timer)
    else
      env.exitcode(1)
    end

actor SyncLeader
  """
  The SyncLeader actor is responsible for creating all of the Pinger
  worker actors and coordinating their activity during each report_pings
  interval.

  Each interval includes the following activity:

  * SyncLeader uses the go() message to all Pinger workers that they
    are permitted to start work.
  * SyncLeader uses ping() messages to trigger a cascade of ping()
    activity that will continue in a Pinger -> Pinger pattern.
  * When the interval timer fires, SyncLeader uses the stop() message
    to tell all Pinger workers to stop sending messages and let any
    "in flight" messages to be received without creating new ping
    messages.
  * The SyncLeader asks all Pinger workers to report the count of
    ping messages the Pinger had received during the work interval.
  """
  let _env: Env
  let _initial_pings: USize
  let _ps: Array[Pinger] val
  var _waiting_for: USize = 0
  var _partial_count: U64 = 0
  var _total_count: U64 = 0
  var _current_t: I64 = 0
  var _last_t: I64 = 0
  var _done: Bool = false

  new create(env: Env, num_pingers: I32, initial_pings: USize) =>
    """
    Create the desired number of Pinger actors and then send them
    their initial ping() messages.
    """
    let ps: Array[Pinger] iso = recover ps.create() end

    for i in Range[I32](0, num_pingers) do
      let p = Pinger(env, i, this)
      ps.push(p)
    end
    let ps': Array[Pinger] val = consume ps
    for p in ps'.values() do
      p.set_neighbors(ps')
    end
    _env = env
    _initial_pings = initial_pings
    _ps = ps'
    (let t_s: I64, let t_ns: I64) = Time.now()
    _last_t = to_ns(t_s, t_ns)
    tell_all_to_go(ps', _initial_pings)

  be tick_fired(done: Bool) =>
    """
    The interval timer has fired.  Stop all Pingers and start
    waiting for confirmation that they have stopped.
    """
    _done = done

    (let t_s: I64, let t_ns: I64) = Time.now()
    @printf("%ld.%09ld".cstring(), t_s, t_ns)
    _current_t = to_ns(t_s, t_ns)

    for p in _ps.values() do
      p.stop()
    end
    _partial_count = 0
    _waiting_for = _ps.size()

  be report_stopped(id: I32) =>
    """
    Collect reports from Pingers that they have stopped working.
    If all have finished, then ask them to report their message
    received counts.
    """
    _waiting_for = _waiting_for - 1
    if (_waiting_for is 0) then
      @printf(",".cstring())
      for p in _ps.values() do
        p.report()
      end
      _waiting_for = _ps.size()
    end

  be report_pings(id: I32, count: U64) =>
    """
    Collect message count reports.  If all have reported, then
    calculate the total message rate, then start the next work
    interval.

    We have separated the stop message and report message into
    a two-round synchronous protocol to ensure that ping messages
    from an earlier work interval are not counted in later
    intervals or cause memory consumption.
    """
    _partial_count = _partial_count + count
    _waiting_for = _waiting_for - 1

    if (_waiting_for is 0) then
      let run_ns: I64 = _current_t - _last_t
      let rate: I64 = (_partial_count.i64() * 1_000_000_000) / run_ns
      @printf("%lld,%lld\n".cstring(), run_ns, rate)

      if not _done then
        (let t_s: I64, let t_ns: I64) = Time.now()
        tell_all_to_go(_ps, _initial_pings)
        _total_count = _total_count + _partial_count
        _last_t = to_ns(t_s, t_ns)
        _waiting_for = _ps.size()
      end
    end

  fun to_ns(t_s: I64, t_ns: I64): I64 =>
    (t_s * 1_000_000_000) + t_ns

  fun tag tell_all_to_go(ps: Array[Pinger] val, initial_pings: USize) =>
    """
    Tell all Pinger actors to start work.

    We do this in two phases: first go() then ping().  Otherwise we
    have a race condition: if we send A.go() and A.ping(...), then
    it is possible for A to send B.ping() before B receives a go().
    If this race happens, then B will not include the ping in its
    local message count, and B will also not forward the ping to
    another actor: the message will be lost, and the system won't
    perform the amount of work that we expected it to perform.
    """
    for p in ps.values() do
      p.go()
    end
    for i in Range[USize](0, initial_pings) do
      for p in ps.values() do
        p.ping(42)
      end
    end

actor Pinger
  let _env: Env
  let _id: I32
  let _leader: SyncLeader
  var _ps: Array[Pinger] val = recover _ps.create() end
  var _num_ps: U64 = 0
  var _go: Bool = false
  var _report: Bool = false
  var _count: U64 = 0
  let _rand: Rand

  new create(env: Env, id: I32, leader: SyncLeader) =>
    _env = env
    _id = id
    _leader = leader
    (_, let t2: I64) = Time.now()
    let tsc: U64 = @ponyint_cpu_tick()
    _rand = Rand(tsc, t2.u64())
    // We "prime the pump", discarding the first few random numbers
    _rand.int(100); _rand.int(100); _rand.int(100)

  be set_neighbors(ps: Array[Pinger] val) =>
    _ps = ps
    _num_ps = ps.size().u64()

  be go() =>
    _go = true
    _report = false
    _count = 0

  be stop() =>
    _go = false
    _leader.report_stopped(_id)

  be report() =>
    _report = true
    _leader.report_pings(_id, _count)
    _count = 0

  be ping(payload: I64) =>
    if _go then
      _count = _count + 1
      send_pings()
    else
      // This is a late-arriving ping.  But it should be arriving
      // before we get a report() message from the SyncLeader.
      try
        Assert(_report is false, "Late message, what???")?
      end
    end

  fun ref send_pings() =>
    let n: U64 = _rand.int(_num_ps)
    try
      _ps(n.usize())?.ping(42)
    else
      _env.out.print("Should never happen but did to pinger " + _id.string())
    end

class Tick is TimerNotify
  let _env: Env
  let _sync_leader: SyncLeader
  let _report_count: U64
  var _tick_count: U64 = 0

  new iso create(env: Env, sync_leader: SyncLeader, report_count: U64) =>
    _env = env
    _sync_leader = sync_leader
    _report_count = report_count

    fun ref apply(timer: Timer, count: U64): Bool =>
      _tick_count = _tick_count + count
      let done = (_report_count > 0) and (_tick_count >= _report_count)
      _sync_leader.tick_fired(done)
      not (done)
