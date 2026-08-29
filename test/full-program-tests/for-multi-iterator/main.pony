actor Main
  new create(env: Env) =>
    let a = [as U32: 1; 2; 3]
    let b = [as U32: 10; 20; 30]

    // Two iterators with destructuring
    var count: U32 = 0
    var sum_x: U32 = 0
    var sum_y: U32 = 0
    for (x, y) in (a.values(), b.values()) do
      count = count + 1
      sum_x = sum_x + x
      sum_y = sum_y + y
    end
    if (count != 3) or (sum_x != 6) or (sum_y != 60) then
      env.exitcode(1)
      return
    end

    // Two iterators with single name (tuple result)
    count = 0
    for xy in (a.values(), b.values()) do
      count = count + 1
    end
    if count != 3 then
      env.exitcode(1)
      return
    end

    // Three iterators
    let c = [as String: "a"; "b"; "c"]
    count = 0
    for (x, y, z) in (a.values(), b.values(), c.values()) do
      count = count + 1
    end
    if count != 3 then
      env.exitcode(1)
      return
    end

    // Shortest iterator wins
    let short = [as U32: 100]
    count = 0
    for (x, y) in (a.values(), short.values()) do
      count = count + 1
    end
    if count != 1 then
      env.exitcode(1)
      return
    end

    // Else clause fires when iterators are empty
    let empty = Array[U32]
    var else_fired: Bool = false
    for (x, y) in (empty.values(), a.values()) do
      env.exitcode(1)
      return
    else
      else_fired = true
    end
    if not else_fired then
      env.exitcode(1)
      return
    end
