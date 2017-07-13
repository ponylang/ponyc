use "cli"
use "collections"
use "time"

class val Config
  let logtable: USize
  let iterate: USize
  let chunk: USize
  let streamer_count: USize
  let updater_count: USize

  new val create(env: Env) ? =>
    let cs = CommandSpec.parent("gups_basic", "", [
      OptionSpec.i64("table", "Log2 of the total table size." where default' = 20)
      OptionSpec.i64("iterate", "Number of iterations." where default' = 10000)
      OptionSpec.i64("chunk", "Chunk size." where default' = 1024)
      OptionSpec.i64("streamers", "Number of streamers." where default' = 4)
      OptionSpec.i64("updaters", "Number of updaters." where default' = 8)
    ])?.>add_help()?
    let cmd =
      match CommandParser(cs).parse(env.args, env.vars())
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
    logtable = cmd.option("table").i64().usize()
    iterate = cmd.option("iterate").i64().usize()
    chunk = cmd.option("chunk").i64().usize()
    streamer_count = cmd.option("streamers").i64().usize()
    updater_count = cmd.option("updaters").i64().usize()

actor Main
  let _env: Env
  var _streamer_count: USize = 0
  var _updater_count: USize = 0
  var _updates: USize = 0
  var _to: Array[Updater] val
  var _start: U64

  new create(env: Env) =>
    _env = env
    _to = recover Array[Updater] end
    _start = Time.nanos()

    try
      let c = Config(env)?
      _streamer_count = c.streamer_count
      _updater_count = c.updater_count

      var size = (1 << c.logtable) / _updater_count
      _updates = c.chunk * c.iterate * _streamer_count

      let count = _updater_count

      _to = recover
        var updaters = Array[Updater](count)

        for i in Range(0, count) do
          updaters.push(Updater(i, size))
        end

        consume updaters
      end

      for i in Range(0, _streamer_count) do
        Streamer(this, _to, size, c.chunk, c.chunk * c.iterate * i)(c.iterate)
      end
    end

  be streamer_done() =>
    if (_streamer_count = _streamer_count - 1) == 1 then
      for u in _to.values() do
        u.done(this)
      end
    end

  be updater_done() =>
    if (_updater_count = _updater_count - 1) == 1 then
      var elapsed = (Time.nanos() - _start).f64()
      var gups = _updates.f64() / elapsed
      _env.out.print(
        "Time: " + (elapsed / 1e9).string() + " GUPS: " + gups.string()
        )
    end


actor Streamer
  let main: Main
  let updaters: Array[Updater] val
  let shift: U64
  let mask: U64
  let chunk: USize
  let rand: PolyRand

  new create(main': Main, updaters': Array[Updater] val, size: USize,
    chunk': USize, seed: USize)
  =>
    main = main'
    updaters = updaters'
    shift = (size.bitwidth() - size.clz()).u64()
    mask = (updaters.size() - 1).u64()
    chunk = chunk'
    rand = PolyRand(seed.u64())

  be apply(iterate: USize) =>
    let upts = updaters.size()
    let chks = chunk

    var list = recover Array[Array[U64] iso](upts) end

    for i in Range(0, upts) do
      list.push(recover Array[U64](chks) end)
    end

    try
      for i in Range(0, chks) do
        var datum = rand()
        var updater = ((datum >> shift) and mask).usize()
        list(updater)?.push(datum)
      end

      var vlist: Array[Array[U64] iso] val = consume list

      for i in vlist.keys() do
        var data = vlist(i)?

        if data.size() > 0 then
          updaters(i)?(data)
        end
      end
    end

    if iterate > 0 then
      apply(iterate - 1)
    else
      main.streamer_done()
    end

actor Updater
  var table: Array[U64] ref

  new create(index: USize, size: USize) =>
    table = Array[U64](size)

    var offset = index.u64() * size.u64()

    for i in Range[U64](0, size.u64()) do
      table.push(i + offset)
    end

  be apply(data: Array[U64] val) =>
    try
      for datum in data.values() do
        var i = (datum and (table.size() - 1).u64()).usize()
        table(i)? = table(i)? xor datum
      end
    end

  be done(main: Main) => main.updater_done()

class PolyRand
  let poly: U64
  let period: U64
  var last: U64

  new create(seed: U64) =>
    poly = 7
    period = 1317624576693539401
    last = 1
    _seed(seed)

  fun ref apply(): U64 =>
    last = (last << 1) xor
      if (last and (1 << 63)) != 0 then poly else 0 end

  fun ref _seed(seed: U64) =>
    var n = seed % period

    if n == 0 then
      last = 1
    else
      var m2 = Array[U64](64)
      last = 1

      for i in Range(0, 63) do
        m2.push(last)
        apply()
        apply()
      end

      var i: U64 = 64 - n.clz()
      last = 2

      while i > 0 do
        var temp: U64 = 0

        for j in Range(0, 64) do
          if ((last >> j.u64()) and 1) != 0 then
            try temp = temp xor m2(j)? end
          end
        end

        last = temp
        i = i - 1

        if ((n >> i) and 1) != 0 then
          apply()
        end
      end
    end
