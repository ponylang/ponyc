use "collections"

class Bar
  fun _final() =>
    @printf[I32]("Bar died\n".cstring())

class Foo
  fun _final() =>
    Bar
    @printf[I32]("Foo died\n".cstring())

actor Main
  new create(env: Env) =>
    Foo

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
