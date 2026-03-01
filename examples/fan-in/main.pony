"""
A microbenchmark for testing thundering herd/fan-in type workloads and how
backpressure impacts them in the Pony runtime. Based on `message-ubench` and
the description in issue #2980 to reproduce the thundering herd/fan-in behavior
in issue #2980.

The topology of this microbenchmark is the following:

  N `Sender` actors => M `Analyzer` actors => 1 `Receiver` actor

The logic is as follows:

* The `Sender` actors send messages as fast as they can to the `Analyzer`
  actors. The number of `Sender` actors is controlled by the `--senders` cli
  argument.
* The `Analyzer` actors receive messages from `Sender` actors and increment a
  count. They only send messages to the `Receiver` actor when a tick fires. The
  number of `Analyzer` actors is controlled by the `--analyzers` cli argument.
* The `Receiver` actor receives messages from the `Analyzer` actors and does
  some "work" (simulated by `usleep`). The amount of "work" is controlled by the
  `--receiver-workload` cli argument.
* The `Coordinator` actor manages when ticks get fired using a timer and when a
  tick is fired it asks all `Analyzer` actors for a status. If an `Analyzer`
  actor is muted due to sending to the `Receiver` actor, it will not respond
  promptly and the reports printed by the `Coordinator` actor will go up and
  down as backpressure kicks in and out when the `Receiver` actor falls behind
  and catches up.
"""

use "assert"
use "cli"
use "collections"
use "random"
use "time"
use @printf[I32](fmt: Pointer[U8] tag, ...)
use @CreateWaitableTimerW[Pointer[U8]](attrs: Pointer[None] tag,
  manual_reset: I32, timer_name: Pointer[None] tag) if windows
use @usleep[I32](micros: U32) if not windows
use @SetWaitableTimer[Bool](handle: Pointer[None] tag, due_time: Pointer[I64] tag,
  period: I32, completion_routine_fn: Pointer[None] tag, completion_routine_arg: Pointer[None] tag,
  resume: I32) if windows
use @WaitForSingleObject[U32](handle: Pointer[None] tag, millis: U32) if windows
use @CloseHandle[Bool](handle: Pointer[None]) if windows

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
            OptionSpec.i64("senders",
              "Number of sender actors"
              where default' = 100)
            OptionSpec.i64("analyzers",
              "Number of analyzer actors"
              where default' = 1000)
            OptionSpec.i64("analyzer-interval",
              "How often analyzers send messages to receiver in centiseconds (10 centiseconds = 1 second)"
              where default' = 100)
            OptionSpec.i64("analyzer-report-count",
              "Number of times analyzers send messages to receiver before shutting down, 0 is infinite"
              where default' = 10)
            OptionSpec.i64("receiver-workload",
              "Number of microseconds the receiver takes to process each message it receives"
              where default' = 10000)
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

      let num_senders = cmd.option("senders").i64()
      let num_analyzers = cmd.option("analyzers").i64()
      let analyzer_interval = cmd.option("analyzer-interval").i64()
      let analyzer_report_count = cmd.option("analyzer-report-count").i64().u64()
      let receiver_workload = cmd.option("receiver-workload").i64().u64()

      env.out.print("# " +
        "senders " + num_senders.string() + ", " +
        "analyzers " + num_analyzers.string() + ", " +
        "analyzer-interval " + analyzer_interval.string() + ", " +
        "analyzer-report-count " + analyzer_report_count.string() + ", " +
        "receiver-workload " + receiver_workload.string())
      env.out.print("time,run-ns,rate")

      let coordinator = Coordinator(env,
        num_senders.i32(), num_analyzers.i32(), analyzer_report_count, receiver_workload)

      let interval: U64 = (analyzer_interval.u64() * 1_000_000_000) / 10
      let timers = Timers
      let timer = Timer(Tick(env, coordinator, analyzer_report_count), interval, interval)
      timers(consume timer)
    else
      env.exitcode(1)
    end


actor Coordinator
  let _receiver: Receiver
  let _analyzers: Array[Analyzer] val
  let _senders: Array[Sender] val
  var _current_t: I64 = 0
  var _last_t: I64 = 0
  let _set_analyzers: Map[I64, (U64, U64)]
  let _num_analyzers: U64
  let _env: Env
  var _done: Bool = false


  new create(env: Env, num_senders: I32, num_analyzers: I32, analyzer_report_count: U64, receiver_workload: U64) =>
    _receiver = Receiver(receiver_workload)
    _set_analyzers = Map[I64, (U64, U64)].create()
    _num_analyzers = num_analyzers.u64()
    _env = env

    var i: I32 = 0
    let analyzers: Array[Analyzer] iso = recover Array[Analyzer](num_analyzers.usize()) end

    while (i < num_analyzers) do
      analyzers.push(Analyzer(_receiver))
      i = i + 1
    end

    _analyzers = consume analyzers


    i = 0
    let senders: Array[Sender] iso = recover Array[Sender](num_senders.usize()) end

    while (i < num_senders) do
      senders.push(Sender(_analyzers))
      i = i + 1
    end

    _senders = consume senders

    (let t_s: I64, let t_ns: I64) = Time.now()
    _last_t = to_ns(t_s, t_ns)
    _current_t = _last_t

  be tick_fired(done: Bool, tick_count: U64) =>
    _last_t = _current_t

    (let t_s: I64, let t_ns: I64) = Time.now()
    _current_t = to_ns(t_s, t_ns)

    for analyzer in _analyzers.values() do
      analyzer.tick_fired(this, _current_t, _last_t)
    end

    if done then
      for sender in _senders.values() do
        sender.done()
      end
      _done = done
    end

  fun to_ns(t_s: I64, t_ns: I64): I64 =>
    (t_s * 1_000_000_000) + t_ns


  be msg_from_analyzer(a: Analyzer, num_msgs: U64, ts: I64, old_ts: I64) =>
    (var num_received, var total_msgs) = 
      try
        _set_analyzers(ts)?
      else
        (0, 0)
      end

    num_received = num_received + 1
    total_msgs = total_msgs + num_msgs

    _set_analyzers(ts) = (num_received, total_msgs)

    if num_received == _num_analyzers then
      let run_ns: I64 = ts - old_ts
      let rate: I64 = (total_msgs.i64() * 1_000_000_000) / run_ns
      _env.out.print(ts.string() + "," + run_ns.string() + "," + rate.string())

      if _done and (ts == _current_t) then
        _env.out.print("Done with message sending... Waiting for Receiver to work through its backlog...")
      end

      try
        _set_analyzers.remove(ts)?
      end
    end


actor Receiver
  let _workload: U32

  new create(workload: U64) =>
    _workload = workload.u32()

  be msg_from_analyzer() =>
    ifdef windows then
      // There is no usleep() on Windows
      var countdown: I64 = -10 * _workload.i64()
      let timer = @CreateWaitableTimerW(USize(0), I32(1), USize(0))
      @SetWaitableTimer(timer, addressof countdown, I32(0), USize(0), USize(0), I32(0))
      @WaitForSingleObject(timer, U32(0xFFFFFFFF))
      @CloseHandle(timer)
    else
      @usleep(_workload)
    end



actor Analyzer
  var _msgs_received: U64 = 0
  let _receiver: Receiver

  new create(receiver: Receiver) =>
    _receiver = receiver

  be msg_from_sender() =>
    _msgs_received = _msgs_received + 1

  be tick_fired(coordinator: Coordinator, ts: I64, old_ts: I64) =>
    coordinator.msg_from_analyzer(this, _msgs_received, ts, old_ts)
    _receiver.msg_from_analyzer()
    _msgs_received = 0



actor Sender
  let _analyzers: Array[Analyzer] val
  var _done: Bool = false
  let _rand: Rand = Rand()

  new create(analyzers: Array[Analyzer] val) =>
    _analyzers = analyzers
    send_msgs()

  be send_msgs() =>
    try
      _analyzers(_rand.int_unbiased[USize](_analyzers.size()))?.msg_from_sender()
    else
      @printf("BBBBAAADDDD\n".cstring())
    end

    if not _done then
      send_msgs()
    end

  be done() =>
    _done = true



class Tick is TimerNotify
  let _env: Env
  let _coordinator: Coordinator
  let _report_count: U64
  var _tick_count: U64 = 0

  new iso create(env: Env, coordinator: Coordinator, report_count: U64) =>
    _env = env
    _coordinator = coordinator
    _report_count = report_count

    fun ref apply(timer: Timer, count: U64): Bool =>
      _tick_count = _tick_count + count
      let done = (_report_count > 0) and (_tick_count >= _report_count)
      _coordinator.tick_fired(done, _tick_count)
      not (done)
