// Embedded objects that reference themselves, or each other, form cycles that
// a collection reaches through pointers into the middle of their allocations.
// The collection must finish.
class C
  var other: (C | None) = None
  var n: U64 = 7

class A
  embed c: C = C

actor Main
  var _self: C
  var _pair: C
  var _i: USize = 0

  new create(env: Env) =>
    let a: A ref = A
    a.c.other = a.c
    _self = a.c
    let b: A ref = A
    let d: A ref = A
    b.c.other = d.c
    d.c.other = b.c
    _pair = b.c
    churn()

  be churn() =>
    var k: USize = 0
    while k < 1000 do
      let s = String(64)
      s.append("abcdefgh")
      k = k + 1
    end
    _i = _i + 1
    if _i < 50 then churn() end
