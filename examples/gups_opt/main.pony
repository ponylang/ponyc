use "cli"
use "collections"
use "time"

class val Config
  let logtable: USize
  let iterate: USize
  let logchunk: USize
  let logactors: USize

  new val create(env: Env) ? =>
    let cs = CommandSpec.parent("gups_opt", "", [
      OptionSpec.i64("table", "Log2 of the total table size."
        where default' = 20)
      OptionSpec.i64("iterate", "Number of iterations." where default' = 10000)
      OptionSpec.i64("chunk", "Log2 of the chunk size." where default' = 10)
      OptionSpec.i64("actors", "Log2 of the actor count." where default' = 2)
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
    logchunk = cmd.option("chunk").i64().usize()
    logactors = cmd.option("actors").i64().usize()

    env.out.print(
      "logtable: " + logtable.string() +
      "\niterate: " + iterate.string() +
      "\nlogchunk: " + logchunk.string() +
      "\nlogactors: " + logactors.string())

actor Main
  let _env: Env
  var _updates: USize = 0
  var _confirm: USize = 0
  var _start: U64 = 0
  var _actors: Array[Updater] val

  new create(env: Env) =>
    _env = env

    let c = try
      Config(env)?
    else
      _actors = recover Array[Updater] end
      return
    end

    let actor_count = 1 << c.logactors
    let loglocal = c.logtable - c.logactors
    let chunk_size = 1 << c.logchunk
    let chunk_iterate = chunk_size * c.iterate

    _updates = chunk_iterate * actor_count
    _confirm = actor_count

    var updaters = recover Array[Updater](actor_count) end

    for i in Range(0, actor_count) do
      updaters.push(Updater(this, actor_count, i, loglocal, chunk_size,
        chunk_iterate * i))
    end

    _actors = consume updaters
    _start = Time.nanos()

    for a in _actors.values() do
      a.start(_actors, c.iterate)
    end

  be done() =>
    if (_confirm = _confirm - 1) == 1 then
      for a in _actors.values() do
        a.done()
      end
    end

  be confirm() =>
    _confirm = _confirm + 1

    if _confirm == _actors.size() then
      let elapsed = (Time.nanos() - _start).f64()
      let gups = _updates.f64() / elapsed

      _env.out.print(
        "Time: " + (elapsed / 1e9).string() +
        "\nGUPS: " + gups.string()
        )
    end

actor Updater
  let _main: Main
  let _index: USize
  let _updaters: USize
  let _chunk: USize
  let _mask: USize
  let _loglocal: USize

  let _output: Array[Array[U64] iso]
  let _reuse: List[Array[U64] iso] = List[Array[U64] iso]
  var _others: (Array[Updater] val | None) = None
  var _table: Array[U64]
  var _rand: U64

  new create(main:Main, updaters: USize, index: USize, loglocal: USize,
    chunk: USize, seed: USize)
  =>
    _main = main
    _index = index
    _updaters = updaters
    _chunk = chunk
    _mask = updaters - 1
    _loglocal = loglocal

    _rand = PolyRand.seed(seed.u64())
    _output = _output.create(updaters)

    let size = 1 << loglocal
    _table = Array[U64].>undefined(size)

    var offset = index * size

    try
      for i in Range(0, size) do
        _table(i)? = (i + offset).u64()
      end
    end

  be start(others: Array[Updater] val, iterate: USize) =>
    _others = others
    iteration(iterate)

  be apply(iterate: USize) =>
    iteration(iterate)

  fun ref iteration(iterate: USize) =>
    let chk = _chunk

    for i in Range(0, _updaters) do
      _output.push(
        try
          _reuse.pop()?
        else
          recover Array[U64](chk) end
        end
        )
    end

    for i in Range(0, _chunk) do
      var datum = _rand = PolyRand(_rand)
      var updater = ((datum >> _loglocal.u64()) and _mask.u64()).usize()

      try
        if updater == _index then
          _table(i)? = _table(i)? xor datum
        else
          _output(updater)?.push(datum)
        end
      end
    end

    try
      let to = _others as Array[Updater] val

      repeat
        let data = _output.pop()?

        if data.size() > 0 then
          to(_output.size())?.receive(consume data)
        else
          _reuse.push(consume data)
        end
      until _output.size() == 0 end
    end

    if iterate > 1 then
      apply(iterate - 1)
    else
      _main.done()
    end

  be receive(data: Array[U64] iso) =>
    try
      for i in Range(0, data.size()) do
        let datum = data(i)?
        var j = ((datum >> _loglocal.u64()) and _mask.u64()).usize()
        _table(j)? = _table(j)? xor datum
      end

      data.clear()
      _reuse.push(consume data)
    end

  be done() =>
    _main.confirm()

primitive PolyRand
  fun apply(prev: U64): U64 =>
    (prev << 1) xor if prev.i64() < 0 then _poly() else 0 end

  fun seed(from: U64): U64 =>
    var n = from % _period()

    if n == 0 then
      return 1
    end

    var m2 = Array[U64].>undefined(64)
    var temp = U64(1)

    try
      for i in Range(0, 64) do
        m2(i)? = temp
        temp = this(temp)
        temp = this(temp)
      end
    end

    var i: U64 = 64 - n.clz()
    var r = U64(2)

    try
      while i > 0 do
        temp = 0

        for j in Range[U64](0, 64) do
          if ((r >> j) and 1) != 0 then
            temp = temp xor m2(j.usize())?
          end
        end

        r = temp
        i = i - 1

        if ((n >> i) and 1) != 0 then
          r = this(r)
        end
      end
    end
    r

  fun _poly(): U64 => 7

  fun _period(): U64 => 1317624576693539401
