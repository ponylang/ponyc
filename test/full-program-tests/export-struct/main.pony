use @get_point_sum[I64](p: Point)

struct \c_api\ val Point
  let x: I64
  let y: I64

  new val create(x': I64, y': I64) =>
    x = x'
    y = y'

  fun val sum(): I64 =>
    x + y

actor Main
  new create(env: Env) =>
    let p: Point val = Point(17, 25)
    let result = @get_point_sum(p)

    if result == 42 then
      env.exitcode(0)
    else
      env.exitcode(1)
    end
