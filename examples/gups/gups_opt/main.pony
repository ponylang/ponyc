use "options"
use "time"
use "collections"

actor Main
  let _env:Env

  var _logtable: U64 = 20
  var _iterate: U64 = 10000
  var _chunk: U64 = 1024
  var _actor_count: U64 = 4

  var updates: U64
  var actors: Array[Updater] val
  var start: U64

  new create(env: Env) =>
    _env = env
    updates = 0
    actors = recover Array[Updater] end //init tracking...

    start = Time.nanos()

    try
      arguments()

      var size = (U64(1) << _logtable) / _actor_count
      updates = _chunk * _iterate * _actor_count

      let count = _actor_count
      let chunk = _chunk
      let iterate = _iterate

      var updaters = recover Array[Updater] end
      updaters.reserve(count)

      for i in Range[U64](0, count) do
        updaters.append(Updater(this, _actor_count, i, size, chunk,
          chunk * iterate * i))
      end

      actors = consume updaters

      for a in actors.values() do
        a.neighbours(actors)
      end

      for a in actors.values() do
        a(iterate)
      end
    else
      usage()
    end

  be done() =>
    if (_actor_count = _actor_count - 1) == 1 then
      try
        for a in actors.values() do
          a.done()
        end
      end
    end

  be confirm() =>
    _actor_count = _actor_count + 1
    if _actor_count == actors.size() then
      let elapsed = (Time.nanos() - start).f64()
      let gups = updates.f64() / elapsed
      _env.out.print("Time: " + (elapsed / 1e9).string() +
        " GUPS: " + gups.string())
    end

  fun ref arguments() ? =>
    var options = Options(_env)

    options
      .add("logtable", "l", None, I64Argument)
      .add("iterate", "i", None, I64Argument)
      .add("chunk", "c", None, I64Argument)
      .add("actors", "a", None, I64Argument)

    for option in options do
      match option
      | ("logtable", var arg: I64) => _logtable = arg.u64()
      | ("iterate", var arg: I64) => _iterate = arg.u64()
      | ("chunk", var arg: I64) => _chunk = arg.u64()
      | ("actors", var arg: I64) => _actor_count = arg.u64()
      | ParseError => usage() ; error
      end
    end

  fun ref usage() =>
    _env.out.print(
      """
      gups_opt [OPTIONS]
        --logtable  N   log2 of the total table size. Defaults to 20.
        --iterate   N   number of iterations. Defaults to 10000.
        --chunk     N   chunk size. Defaults to 1024.
        --actors    N   number of actors. Defaults to 4.
      """
      )

actor Updater
  let main: Main
  let index: U64
  let shift: U64
  let updaters: U64
  let mask: U64
  let chunk: U64
  let rand: PolyRand
  let reuse: List[Array[U64] iso]
  var others: (Array[Updater] val | None)
  var table: Array[U64]

  new create(main':Main, updaters': U64, index': U64, size: U64, chunk': U64,
    seed: U64)
    =>
    main = main'
    index = index'
    shift = size.width() - size.clz()
    updaters = updaters'
    mask = updaters - 1
    chunk = chunk'
    rand = PolyRand(seed)
    reuse = List[Array[U64] iso]
    others = None
    table = Array[U64].undefined(size)

    var offset = index * size

    for i in Range[U64](0, size - 1) do
      try table(i) = i + offset end
    end

  be neighbours(others': Array[Updater] val) =>
    others = others'

  be apply(iterate: U64) =>
    let count = updaters
    let chk = chunk

    var array = Array[Array[U64] iso](count)

    for i in Range(0, updaters) do
      array.append(
        try
          reuse.pop()
        else
          recover Array[U64](chk) end
        end
        )
    end

    for i in Range(0, chunk) do
      var datum = rand()
      var updater = (datum >> shift) and mask

      try
        if updater == index then
          table(i) = table(i) xor datum
        else
          array(updater).append(datum)
        end
      end
    end

    for i in Range(0, array.size()) do
      try
        let data = array.delete(i)

        if data.size() > 0 then
          match others
          | var next: Array[Updater] val =>
            try next(i).receive(consume data) end
          end
        end
      end
    end

    if iterate > 0 then
      apply(iterate - 1)
    else
      main.done()
    end

  be receive(data: Array[U64] iso) =>
    try
      for i in Range(0, data.size()) do
        let datum = data(i)
        var j = datum and (data.size() - 1)
        table(j) = table(j) xor datum
      end
      data.clear()
      reuse.push(consume data)
    end

  be done() =>
    main.confirm()

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
      if (last and (U64(1) << 63)) != 0 then poly else U64(0) end

  fun ref _seed(seed: U64) =>
    var n = seed % period

    if n == 0 then
      last = 1
    else
      var m2 = Array[U64](64)
      last = 1

      for i in Range(0, 63) do
        m2.append(last)
        apply()
        apply()
      end

      var i: U64 = 64 - n.clz()
      last = U64(2)

      while i > 0 do
        var temp: U64 = 0

        for j in Range(0, 64) do
          if ((last >> j) and 1) != 0 then
            try temp = temp xor m2(j) end
          end
        end

        last = temp
        i = i - 1

        if ((n >> i) and 1) != 0 then
          apply()
        end
      end
    end
