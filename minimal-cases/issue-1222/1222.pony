use "collections"

actor Main
  new create(env: Env) =>
    var list = List[I32]

    while list.size() < 2 do
      env.out.print("could be infinite loop if the for loop is optimized out")

      for d in Range[I32](0, 2) do
        env.out.print("running for loop...")
        list.push(d)
      end
    end

    env.out.print("finished!")
