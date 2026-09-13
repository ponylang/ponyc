use "lib:ffi-struct-by-value-additional"

use @point_sum[F64](p: Point \by_value\)
use @point_make[Point \by_value\](x: F64, y: F64)
use @point_diff_sum[F64](a: Point \by_value\, b: Point \by_value\)
use @scale_point[Point \by_value\](p: Point \by_value\, factor: F64)
use @color_max[U8](c: Color \by_value\)
use @color_make[Color \by_value\](r: U8, g: U8, b: U8)
use @rect_area[F32](r: Rect \by_value\)
use @rect_move[Rect \by_value\](r: Rect \by_value\, dx: I32, dy: I32)
use @pony_exitcode[None](code: I32)

struct Point
  var x: F64 = 0
  var y: F64 = 0

struct Color
  var r: U8 = 0
  var g: U8 = 0
  var b: U8 = 0

struct Rect
  var x: I32 = 0
  var y: I32 = 0
  var w: F32 = 0
  var h: F32 = 0

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

    let c: Color ref = Color
    c.r = 10
    c.g = 25
    c.b = 5
    let cmax = @color_max(c)

    let c2 = @color_make(3, 7, 2)
    let csum = c2.r.i32() + c2.g.i32() + c2.b.i32()

    let r: Rect ref = Rect
    r.x = 0
    r.y = 0
    r.w = 5
    r.h = 6
    let area = @rect_area(r)

    let r2: Rect ref = Rect
    r2.x = 1
    r2.y = 2
    r2.w = 5
    r2.h = 6
    let moved = @rect_move(r2, 10, 20)

    let total = (sum + qsum + dist + scaled_sum).i32()
      + cmax.i32() + csum + area.i32() + moved.x

    @pony_exitcode(total)
