use "collections"

actor Main
  new create(env: Env) =>
    var a = Array[U64](2)

    for i in Range[U64](0, 16000) do
      a.append(i)
    end
