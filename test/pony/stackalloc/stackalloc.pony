use "collections"

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
