"""
Types in a separate package for cross-package HeapToStack testing.
"""

class Point
  let x: U64
  let y: U64
  new create(x': U64, y': U64) => x = x'; y = y'

primitive Geometry
  fun offset_sum(a: Point box, b: Point box): U64 =>
    (a.x + b.x) + (a.y + b.y)
