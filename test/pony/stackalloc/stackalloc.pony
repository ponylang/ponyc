use "collections"

class Foo
  let _env: Env

  new create(env: Env) =>
    _env = env

  fun foo(): Bool =>
    // _env.out.print("hi")
    true

  fun _final() =>
    foo()
    @printf[I32]("Foo died\n".cstring())
    Array[U8].undefined[U8](0).reserve(8)

actor Main
  new create(env: Env) =>
    // TODO: why is the pointer alloc not done on the stack?
    // because it is assigned to the array object?
    var array = Array[U64].undefined(10)

    // var i = U64(0)

    // try
    //   while i < 10 do
    //     array(i) = i
    //   end
    // end

    try
      for i in Range(0, 10) do
        array(i) = i
      end
    end
