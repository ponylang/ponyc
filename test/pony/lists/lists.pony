use "collections"

actor Main
  new create(env: Env) =>
    let list1 = List[U64]
    let list2 = List[U64]

    for i in Range(0, 10) do
      list1.push(i)
    end

    list2.append_list(list1)

    try
      for i in list2.values() do
        env.out.print(i.string())
      end
    end
