actor Spreader
  var _parent: (Spreader | None)
  var _env: Env
  var _count: U64
  var _result: U64
  var _received: U64

  new create(env: Env) =>
    _env = env

    var count: U64 = try _env.args(1).u64() else 10 end

    if count > 0 then
      _count = count
      spawn_child(count)
      spawn_child(count)
    else
      env.stdout.print("1 actor")
    end

  new spread(parent: Spreader, count: U64) =>
    if count == 0 then
      parent.result(1)
    else
      _parent = parent
      _count = count
      _result = 0
      _received = 0

      spawn_child(_count - 1)
      spawn_child(_count - 1)
    end

  be result(i: U64) =>
    _received = _received + 1
    _result = _result + i

    if _received == 2 then
      match _parent
      | var p: Spreader =>
        p.result(_result + 1)
      | var p: None =>
        _env.stdout.print((_result + 1).string() + " actors")
      end
    end

  fun ref spawn_child(count: U64) =>
    Spreader.spread(this, _count - 1)

actor Main
  new create(env: Env) =>
    Spreader(env)
