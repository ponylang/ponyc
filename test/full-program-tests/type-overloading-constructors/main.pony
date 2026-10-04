use @pony_exitcode[None](code: I32)

class Timestamp
  var _epoch: U64

  new create(value: U64) =>
    _epoch = value

  new create(s: String) =>
    _epoch = try s.u64()? else 0 end

  fun get_epoch(): U64 => _epoch

actor Main
  new create(env: Env) =>
    let t1 = Timestamp(U64(100))
    let t2 = Timestamp("200")

    if (t1.get_epoch() == 100) and (t2.get_epoch() == 200) then
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
