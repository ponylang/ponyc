use "lib:ffi-struct-by-value-additional"

use @point_sum[F64](p: Point) \by_value\
use @point_make[Point](x: F64, y: F64) \by_value\
use @point_diff_sum[F64](a: Point, b: Point) \by_value\
use @scale_point[Point](p: Point, factor: F64) \by_value\
use @pony_exitcode[None](code: I32)

struct Point
  var x: F64 = 0
  var y: F64 = 0

actor Main
  new create(env: Env) =>
    let p: Point ref = Point
    p.x = 30
    p.y = 12
    let sum = @point_sum(p)

    let q = @point_make(10, 5)
    let qsum = q.x + q.y

    let dist = @point_diff_sum(p, q)

    let scaled = @scale_point(p, F64(2.0))
    let scaled_sum = scaled.x + scaled.y

    @pony_exitcode((sum + qsum + dist + scaled_sum).i32())
