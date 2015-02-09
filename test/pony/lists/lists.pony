use "collections"

actor Main
  new create(env: Env) =>
    let list1 = List[(String, U64)]
    let list2 = List[(String, U64)]

    for i in Range(0, 10) do
      list1.push((i.string(), i))
    end

    list2.append_list(list1)

    try
      for (s, i) in list2.values() do
        env.out.print(s + ": " + i.string())
      end
    end
