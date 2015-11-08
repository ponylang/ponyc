use "collections"
use "debug"
use "options"
use "time"
use "yield"

class StopWatch
  """
  A simple stopwatch class for performance micro-benchmarking
  """
  var _s : U64 = 0

  fun ref start() : StopWatch =>
    _s = Time.nanos()
    this
  
  fun delta() : U64 =>
    Time.nanos() - _s
     
actor LonelyPony
  """
  A simple manifestation of the lonely pony problem
  """
  var _env : Env
  let _sw : StopWatch = StopWatch 
  var _alive : Bool = true
  var _debug : Bool = false
  var _m : U64
  var _n : U64

  new create(env : Env, debug : Bool = false, n : U64 = 0) =>
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
    The trivial case of a badly written behaviour that eats a scheduler ( forever )
    """
    while _alive do
      if _debug then
        _env.out.print("Beep boop!") 
      end
    end

  be perf() =>
    var r = Range[U64](0,_n)
    _sw.start()
    for i in r do
      if _debug then
        _env.err.print("L:" + (_n - i).string())
      end
    end
    let d = _sw.delta()
    _env.out.print("N: " + _m.string() + ", Lonely: " + d.string())
  
actor InterruptiblePony is Yieldable
  """
  An interruptible version that avoids the lonely pony problem
  """
  var _env : Env
  let _sw : StopWatch = StopWatch
  var _alive : Bool = true
  var _debug : Bool = false
  var _n : U64

  new create(env : Env, debug : Bool, n : U64 = 0) =>
    _env = env
    _debug = debug
    _n = n

  be kill() =>
    if _debug then
      _env.err.print("Received kill signal!")
    end
    _alive = false

  be _forever() =>
    match _alive
    | true => 
      Debug.err("Beep boop!")
      this.forever()
    | false =>
      Debug.err("Ugah!")
      None
    end
  be forever() =>
    var that = recover this end
    this.yield(lambda iso()(that) => that._forever() end)

  be _bare_perf() =>
    match _n
    | 0 => 
      Debug.err("Ugah!")
      let d = _sw.delta()
      _env.out.print("N=" + _n.string() + ", Interruptible: " + d.string())
    else
      if _debug then
        _env.err.print("I: " + _n.string())
      end
      _n = _n - 1
      this._inst_perf()
    end
  be _inst_perf() =>
    let that = recover this end
    this.yield(lambda iso()(that) => that._bare_perf() end)

  be perf() =>
    _sw.start()
    _inst_perf()
    this
  
actor PunkDemo is Yieldable
  var _env : Env
  var _alive : Bool = false
  var _current : U8 = 0

  new create(env : Env) =>
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

  be _loop() =>
    match _alive
    | true => this.loop()
    | false => _env.out.print("Done! ") ; None
    end

  be loop() =>
    let that = recover this end
    this.yield(lambda iso()(that) => that._loop() end)

actor Main
  var _env : Env
  be create(env : Env) =>
    _env = env

    var punk : Bool = false
    var lonely : Bool = false
    var perf : U64 = 0
    var debug : Bool = false
    var err : Bool = false 

    let options = Options(env) +
      ("punk", "l", None) +
      ("lonely", "l", None) +
      ("bench", "b", I64Argument) +
      ("debug", "d", None)
      ("help", "h", None)

    for opt in options do
      match opt
      | ("punk", None) => punk = true
      | ("lonely", None) => lonely = true
      | ("bench", let arg : I64) => perf = arg.u64()
      | ("debug", None) => debug = true
      else
        err = true
      end
    end

    match err
    | true =>
      usage() 
      return None
    else
      match punk
      | true =>
        PunkDemo(env)
          .loop()
          .inc().inc().inc()
          .dec().dec().dec()
          .inc().dec()
          .kill()
      else
        match perf > 0
        | true =>
          InterruptiblePony(env,debug,perf).perf() 
          LonelyPony(env,debug,perf).perf() 
        else
          match lonely
          | false => InterruptiblePony(env,debug).forever().kill()
          | true => LonelyPony(env,debug).forever().kill()
          end
        end
      end
    end

  fun usage() =>
    _env.out.print(
    """
      yield ( --punk | --lonely | --bench ) [ OPTIONS ]
        --punk, -p      Run a punctuated stream demonstration
        --lonely, -l    Run a non-interruptible behaviour with logic that runs forever
        --bench, -b     Run an instrumented behaviour to guesstimate overhead of non/interruptive

      OPTIONS
        --debug, -d     Run in debug mode with verbose output
        --help, -h      This help
  
      DESCRIPTION

      Demonstrate use of the yield package's Yieldable trait so that actors
      can be written with long lived interruptible behaviours.

      By Default, the actor will run quiet and interruptibly.
    """)
      
