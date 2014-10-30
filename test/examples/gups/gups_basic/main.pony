use "random"

actor Main
  let _env: Env
  var _logtable: U64 = 20
  var _iterate: U64 = 10000
  var _chunk: U64 = 1024
  var _streamer_count: U64 = 4
  var _updater_count: U64 = 8
  var _updates: U64 = 0
  var _to: Array[Updater] val
  //var _start: Time

  new create(env:Env) =>
    _env = env

    _to = recover Array[Updater] end //init tracking...

    try
      arguments()

      //without U64(1), error is: Cannot apply shl to floats?
      var size = (U64(1) << _logtable) / _updater_count
      _updates = _chunk * _iterate * _streamer_count

      let count = _updater_count

      _to = recover
        var updaters = Array[Updater]
        updaters.reserve(count)

        for i in Range[U64](0, count) do
          updaters.append(Updater(i, size))
        end

        consume updaters
      end

      //_start = Time.now()

      for i in Range[U64](0, _streamer_count) do
        Streamer(this, _to, size, _chunk, _chunk * _iterate * i)(_iterate)
      end
    else
      usage()
    end

  be streamer_done() =>
    if (_streamer_count = _streamer_count - 1) == 1 then
      try
        for u in _to.values() do
          u.done(this)
        end
      end
    end

  be updater_done() =>
    if (_updater_count = _updater_count - 1) == 1 then
      var runtime: U64 = 0 /*Time.now() - _start*/
      var gups = _updates.f32() / runtime.f32() / F32(1e9)
      _env.stdout.print("Time: " + runtime.string() + " GUPS: " + gups.string())
    end

  fun ref arguments() ? =>
    var n: U64 = 1

    while n < _env.args.length() do
      var option = _env.args(n)
      var value = _env.args(n + 1)
      n = n + 2

      match option
      | "--logtable" => _logtable = value.u64()
      | "--iterate" => _iterate = value.u64()
      | "--chunk" => _chunk = value.u64()
      | "--streamers" => _streamer_count = value.u64()
      | "--updaters" => _updater_count = value.u64()
      else
        error
      end
    end

  fun ref usage() =>
    _env.stdout.print(
      """
      gups_basic [OPTIONS]
        --logtable  N   log2 of the total table size. Defaults to 20."
        --iterate   N   number of iterations. Defaults to 10000."
        --chunk     N   chunk size. Defaults to 1024."
        --streamers N   number of streamers. Defaults to 4."
        --updaters  N   number of updaters. Defaults to 8."
      """
      )

actor Streamer
  let main:Main
  let updaters:Array[Updater] val
  let shift:U64
  let mask:U64
  let chunk:U64
  let rand: MT

  new create(main':Main, updaters':Array[Updater] val, size:U64, chunk':U64,
    seed:U64) =>
    main = main'
    updaters = updaters'
    shift = size/*.log2()*/ //TODO
    mask = updaters.length() - 1
    chunk = chunk'
    rand = MT(seed)

  be apply(iterate:U64) =>
    let upts = updaters.length()
    let chks = chunk

    var list = recover Array[Array[U64] iso].prealloc(upts) end

    for i in Range[U64](0, upts) do
      list.append(recover Array[U64].prealloc(chks) end)
    end

    try
      for i in Range[U64](0, chks) do
        var datum = rand.next()
        var updater = (datum >> shift) and mask
        list(updater).append(datum)
      end

      var vlist:Array[Array[U64] iso] val = consume list

      for i in vlist.keys() do
        var data = vlist(i)

        if data.length() > 0 then
          updaters(i)(data)
        end
      end
    end

    if iterate > 0 then
      apply(iterate - 1)
    else
      main.streamer_done()
    end

actor Updater
  var table:Array[U64] ref

  new create(index:U64, size:U64) =>
    table = Array[U64].init(size, 0)

    var offset = index * size;
    for i in Range[U64](0, size - 1) do
      try table(i) = i + offset end
    end

  be apply(data:Array[U64] val) =>
    try
      for datum in data.values() do
        var i = datum and (table.length() - 1)
        table(i) = table(i) xor datum
      end
    end

  be done(main:Main) => main.updater_done()
