actor Main
  new create(env: Env) =>
    var x: U64
    var y: U64

    x = 0
    y = while x < 10 do
      x = x + 1
    else
      0
    end

    env.out.print(y.string() + ", expected 9")

    x = 0
    y = while x < 10 do
      if x == 3 then
        break 99
      end
      x = x + 1
    else
      0
    end

    env.out.print(y.string() + ", expected 99")

    x = 0
    y = while x < 10 do
      if x == 9 then
        x = 999
        continue
      end
      x = x + 1
    else
      x
    end

    env.out.print(y.string() + ", expected 999")

    x = 0
    y = repeat
      x = x + 1
    until x >= 10 else
      0
    end

    env.out.print(y.string() + ", expected 9")

    x = 0
    y = repeat
      if x == 3 then
        break 99
      end
      x = x + 1
    until x >= 10 else
      0
    end

    env.out.print(y.string() + ", expected 99")

    x = 0
    y = repeat
      if x == 9 then
        x = 999
        continue
      end
      x = x + 1
    until x >= 10 else
      x
    end

    env.out.print(y.string() + ", expected 999")
