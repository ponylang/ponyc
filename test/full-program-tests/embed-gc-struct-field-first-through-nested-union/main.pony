// A struct's first embedded field, held through a union that also holds an
// intersection. The value's trace tries each type in the union in turn, and a
// value that matches none of the intersection's types is still traced as a
// type that can share the struct's address.
struct S
  embed c: C = C
  var x: U8 = 0

  new create() =>
    None

class C
  var v: U64 = 0

  new create() =>
    None

interface I1
  fun i1(): U8

interface I2
  fun i2(): U8

class val K is (I1 & I2)
  fun i1(): U8 =>
    1

  fun i2(): U8 =>
    2

actor Main
  var _f: (C tag | (I1 val & I2 val))
  var _s: S
  var _n: U32 = 0

  new create(env: Env) =>
    _s = S
    _f = _s.c
    tick()

  be tick() =>
    _n = _n + 1
    let junk = Array[U8].init(0, 4096)
    if _n < 20 then tick() end
