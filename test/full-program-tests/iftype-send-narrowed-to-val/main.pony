use @pony_exitcode[None](code: I32)

class iso T[A: Array[I64] #send]
  var data: A

  new iso create() =>
    data = recover Array[I64] end

  fun ref push(v: I64) =>
    iftype A <: Array[I64] iso then
      data.push(v)
    elseif A <: Array[I64] val then
      data = recover data.clone() .> push(v) end
    end

  fun box size(): USize =>
    iftype A <: Array[I64] val then
      data.size()
    else
      0
    end

actor Main
  new create(env: Env) =>
    let t = T[Array[I64] val].create()
    t.push(42)
    if t.size() == 1 then
      @pony_exitcode(1)
    end
