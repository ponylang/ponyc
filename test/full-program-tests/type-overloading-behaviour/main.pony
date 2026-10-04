use @pony_exitcode[None](code: I32)

actor Counter
  var _count: USize = 0
  let _env: Env

  new create(env: Env) =>
    _env = env

  be increment(s: String) =>
    _count = _count + s.size()
    if _count == 5 then
      @pony_exitcode(0)
    end

  be increment(n: USize) =>
    _count = _count + n
    if _count == 5 then
      @pony_exitcode(0)
    end

actor Main
  new create(env: Env) =>
    @pony_exitcode(1)
    let c = Counter(env)
    c.increment("hello")
