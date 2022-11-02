use @pony_exitcode[None](code: I32)

actor Main
  let _tests_to_run: USize = 1_000
  var _tests_run: USize = 0

  new create(env: Env) =>
    run_test()

  fun run_test() =>
    let arr = "hello, world!".clone()
    (let hello, let rest) = (consume arr).chop(5)

    ArrayGrower(this).grow(consume hello)
    Modifier.mod(consume rest)

  be passed() =>
    _tests_run = _tests_run + 1
    if _tests_run < _tests_to_run then
      run_test()
    end

  be failed() =>
    @pony_exitcode(1)


actor Modifier
  be mod(data: String iso) =>
    try data(data.size()-1)? = '?' end

actor ArrayGrower
  let _m: Main

  new create(m: Main) =>
    _m = m

  be grow(data: String iso) =>
    let arr = (consume data).iso_array()
    arr.undefined(13)
    let final = String.from_iso_array(consume arr)
    if (final.size() == 13) and
      (final.substring(0, 5).eq("hello")) and
      (not final.substring(0, 12).eq("hello, world"))
    then
      _m.passed()
    else
      _m.failed()
    end
