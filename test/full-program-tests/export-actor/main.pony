use @call_value[I64](c: Counter tag)

actor \c_api\ Counter
  var _value: I64 = 0

  new create() => None

  be increment() =>
    _value = _value + 1

  fun tag value(): I64 => 42

actor Main
  new create(env: Env) =>
    let c = Counter
    let result = @call_value(c)

    if result == 42 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
