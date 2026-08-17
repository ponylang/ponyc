use "cli"
use "collections"
use "debug"
use "time"

class StopWatch
  """
  A simple stopwatch class for performance micro-benchmarking
  """
  var _s: U64 = 0

  fun ref start(): StopWatch =>
    _s = Time.nanos()
    this

  fun delta(): U64 =>
    Time.nanos() - _s

actor LonelyPony
  """
  A simple manifestation of the lonely pony problem
  """
  var _env: Env
  let _sw: StopWatch = StopWatch
  var _alive: Bool = true
  var _debug: Bool = false
  var _m: U64
  var _n: U64

  new create(env: Env, debug: Bool = false, n: U64 = 0) =>
    _env = env
    _debug = debug
    _m = n
    _n = n

  be kill() =>
    if _debug then
      _env.out.print("Received kill signal!")
    end
    _alive = false

  be forever() =>
    """
    The trivial case of a badly written behaviour that
    eats a scheduler (forever)
    """
    while _alive do
      if _debug then
        _env.out.print("Beep boop!")
      end
    end

  be perf() =>
    """
    Run a simple loop and time it for benchmarking.
    """
    var r = Range[U64](0,_n)
    _sw.start()
    for i in r do
      if _debug then
        _env.err.print("L:" + (_n - i).string())
      end
    end
    let d = _sw.delta()
    _env.out.print(
      "N: " + _m.string()
        + ", Lonely: " + d.string())

actor InterruptiblePony
  """
  An interruptible version that avoids the lonely pony problem
  """
  var _env: Env
  let _sw: StopWatch = StopWatch
  var _alive: Bool = true
  var _debug: Bool = false
  var _n: U64

  new create(env: Env, debug: Bool, n: U64 = 0) =>
    _env = env
    _debug = debug
    _n = n

  be kill() =>
    if _debug then
      _env.err.print("Received kill signal!")
    end
    _alive = false

  be forever() =>
    match \exhaustive\ _alive
    | true =>
      Debug.err("Beep boop!")
      this.forever()
    | false =>
      Debug.err("Ugah!")
      None
    end

  be _bare_perf() =>
    match _n
    | 0 =>
      Debug.err("Ugah!")
      let d = _sw.delta()
      _env.out.print(
        "N=" + _n.string()
          + ", Interruptible: " + d.string())
    else
      if _debug then
        _env.err.print("I: " + _n.string())
      end
      _n = _n - 1
      this._bare_perf()
    end

  be perf() =>
    _sw.start()
    _bare_perf()
    this

actor PunkDemo
  """
  Demonstrates punctuated stream processing with
  interruptible behaviours.
  """
  var _env: Env
  var _alive: Bool = false
  var _current: U8 = 0

  new create(env: Env) =>
    _env = env

  be inc() =>
    if _current < 255 then
      _current = _current + 1
    end
    print()

  be dec() =>
    if _current > 0 then
      _current = _current - 1
    end
    print()

  fun print() =>
    _env.out.print("Level: " + _current.string())

  be kill() =>
    _alive = false

  be loop() =>
    match \exhaustive\ _alive
    | true => this.loop()
    | false => _env.out.print("Done! ") ; None
    end

actor Main
  var _env: Env

  new create(env: Env) =>
    _env = env

    let cs =
      try
        CommandSpec.leaf(
          "yield",
          """
          Demonstrate use of the yield behaviour when
          writing tail recursive behaviours in pony.

          By Default, the actor will run quiet and
          interruptibly.""",
          [
            OptionSpec.bool(
              "punk",
              "Run a punctuated stream demonstration."
              where short' = 'p', default' = false)
            OptionSpec.i64(
              "bench",
              "Run an instrumented behaviour to "
                + "guesstimate overhead of "
                + "non/interruptive."
              where short' = 'b', default' = 0)
            OptionSpec.bool(
              "lonely",
              "Run a non-interruptible behaviour "
                + "with logic that runs forever."
              where short' = 'l', default' = false)
            OptionSpec.bool(
              "debug",
              "Run in debug mode with verbose output."
              where short' = 'd', default' = false)
          ])? .> add_help()?
      else
        _env.exitcode(-1)
        return
      end

    let cmd =
      match \exhaustive\ CommandParser(cs).parse(
        _env.args, _env.vars)
      | let c: Command => c
      | let ch: CommandHelp =>
        ch.print_help(_env.out)
        _env.exitcode(0)
        return
      | let se: SyntaxError =>
        _env.out.print(se.string())
        _env.exitcode(1)
        return
      end

    var punk: Bool = cmd.option("punk").bool()
    var perf: U64 = cmd.option("bench").i64().u64()
    var lonely: Bool = cmd.option("lonely").bool()
    var debug: Bool = cmd.option("lonely").bool()

    match punk
    | true =>
      PunkDemo(env)
        .> loop()
        .> inc() .> inc() .> inc()
        .> dec() .> dec() .> dec()
        .> inc() .> dec()
        .> kill()
    else
      match perf > 0
      | true =>
        InterruptiblePony(env,debug,perf).perf()
        LonelyPony(env,debug,perf).perf()
      else
        match \exhaustive\ lonely
        | false =>
          InterruptiblePony(env,debug)
            .> forever() .> kill()
        | true =>
          LonelyPony(env,debug)
            .> forever() .> kill()
        end
      end
    end
