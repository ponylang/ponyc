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

    @printf[I32]("%d, expected 9\n".cstring(), y.i32())

    x = 0
    y = while x < 10 do
      if x == 3 then
        break 99
      end
      x = x + 1
    else
      0
    end

    @printf[I32]("%d, expected 99\n".cstring(), y.i32())

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

    @printf[I32]("%d, expected 999\n".cstring(), y.i32())

    x = 0
    y = repeat
      x = x + 1
    until x >= 10 else
      0
    end

    @printf[I32]("%d, expected 9\n".cstring(), y.i32())

    x = 0
    y = repeat
      if x == 3 then
        break 99
      end
      x = x + 1
    until x >= 10 else
      0
    end

    @printf[I32]("%d, expected 99\n".cstring(), y.i32())

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

    @printf[I32]("%d, expected 999\n".cstring(), y.i32())

    var z: U32
    if y > 10 then
      z = 1
    elseif y < 20 then
      z = 2
    else
      z = 3
    end
    z
