use "collections"

actor Ring
  let _id: U32
  let _env: Env
  var _next: (Ring | None)

  new create(id: U32, env: Env, neighbor: (Ring | None) = None) =>
    _id = id
    _env = env
    _next = neighbor

  be set(neighbor: Ring) =>
    _next = neighbor

  be pass(i: U64) =>
    if i > 0 then
      match _next
      | var n: Ring =>
        n.pass(i - 1)
      end
    else
      _env.out.print(_id.string())
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
    var i: U64 = 1

    while i < _env.args.size() do
      // Every option has an argument.
      var option = _env.args(i)
      var value = _env.args(i + 1)
      i = i + 2

      match option
      | "--size" =>
        _ring_size = value.u32()
      | "--count" =>
        _ring_count = value.u32()
      | "--pass" =>
        _pass = value.u64()
      else
        error
      end
    end

  fun setup_ring() =>
    for j in Range[U32](0, _ring_count) do
      let first = Ring(1, _env)
      var next = first

      for k in Range[U32](0, _ring_size - 1) do
        let current = Ring(_ring_size - k, _env, next)
        next = current
      end

      first.set(next)

      if _pass > 0 then
        first.pass(_pass)
      end
    end

  fun usage() =>
    _env.out.print(
      """
      rings OPTIONS
        --size N number of actors in each ring
        --count N number of rings
        --pass N number of messages to pass around each ring
      """
      )
