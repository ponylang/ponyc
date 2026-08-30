use @pony_exitcode[None](code: I32)

class T[A: Array[I64] #any]
  var data: A

  new create(data': A) =>
    data = consume data'

  fun box clone_data(): USize =>
    iftype A <: Array[I64] trn then
      data.clone().size()
    elseif A <: Array[I64] val then
      data.clone().size()
    else
      0
    end

actor Main
  new create(env: Env) =>
    let arr: Array[I64] trn = recover Array[I64] .> push(42) end
    let t = T[Array[I64] trn](consume arr)
    if t.clone_data() == 1 then
      @pony_exitcode(1)
    end
