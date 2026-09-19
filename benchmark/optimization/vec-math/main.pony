"""
Fluent vector math with temporary object chains.

Each iteration creates two Vec3 objects and chains arithmetic operations that
each return a new Vec3 temporary. The expression a.add(b).scale(0.01).sub(...)
produces 4 intermediate Vec3 allocations per iteration (200M total). All
temporaries are dead after each iteration.

This combines HeapToStack pressure (many short-lived objects) with real F64
computation. It tests whether the optimizer can see through method-return
temporaries in a chain and promote the entire sequence.
"""

class Vec3
  let x: F64
  let y: F64
  let z: F64

  new create(x': F64, y': F64, z': F64) =>
    x = x'
    y = y'
    z = z'

  fun add(other: Vec3 box): Vec3 =>
    Vec3(x + other.x, y + other.y, z + other.z)

  fun sub(other: Vec3 box): Vec3 =>
    Vec3(x - other.x, y - other.y, z - other.z)

  fun scale(s: F64): Vec3 =>
    Vec3(x * s, y * s, z * s)

  fun dot(other: Vec3 box): F64 =>
    (x * other.x) + (y * other.y) + (z * other.z)

  fun mag_sq(): F64 =>
    dot(this)

actor Main
  new create(env: Env) =>
    var sum: F64 = 0
    var i: U64 = 0
    while i < 50_000_000 do
      let fi = i.f64()
      let a = Vec3(fi, fi + 1.0, fi + 2.0)
      let b = Vec3(fi * 0.5, fi * 0.3, fi * 0.1)
      let r = a.add(b).scale(0.01).sub(a.scale(0.005))
      sum = sum + r.mag_sq()
      i = i + 1
    end
    env.out.print(sum.string())
