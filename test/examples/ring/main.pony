actor Ring
  var _next: (Ring | None)
  var _stdout: Stdout

  new create(stdout: Stdout) =>
    _next = None
    _stdout = stdout

  new with(neighbor: Ring, stdout: Stdout) =>
    _next = neighbor
    _stdout = stdout

  be set(neighbor: Ring) =>
    _next = neighbor

  be pass(i: U64) =>
    if i > 0 then
      match _next
      | var n: Ring =>
        n.pass(i - 1)
      end
    else
      _stdout.print("done")
    end

actor Main
  var _ring_size: U32 = 3
  var _ring_count: U32 = 1
  var _pass: U64 = 10

  var _env: Env

  new create(env: Env) =>
    _env = env

    try
      parse_args()
      setup_ring()
    else
      usage()
    end

  fun ref parse_args() ? =>
    var i = 1

    while i < _env.args.length() do
      var arg = _env.args(i)
      i = i + 1 //every option has an arg

      match arg
      | "--size" =>
        _ring_size = _env.args(i).u32()
      | "--count" =>
        _ring_count = _env.args(i).u32()
      | "--pass" =>
        _pass = _env.args(i).u64()
      else
        error
      end
    end

  fun box setup_ring() =>
    var first: Ring
    var next: Ring
    var current: Ring

    for j in Range[U32](0, _ring_count) do
      first = Ring(_env.stdout)
      next = first

      for k in Range[U32](0, _ring_size - 1) do
        current = Ring.with(next, _env.stdout)
        next = current
      end

      first.set(next)

      if _pass > 0 then
        first.pass(_pass)
      end
    end

  fun box usage() =>
    _env.stdout.print(
      """
      rings OPTIONS
        --size N number of actors in each ring
        --count N number of rings
        --pass N number of messages to pass around each ring
      """
      )
