// An embedded struct with no size has the address of the object that holds
// it, wherever it is declared, so its address is always inside that object.
struct Z
  new create() => None

// No size either, though it has a field.
struct B
  embed z: Z = Z
  new create() => None

class C
  var x: U64 = 0
  embed z: Z = Z

struct S
  var x: U64 = 0
  embed z: Z = Z
  new create() => None

struct \packed\ P
  var x: U8 = 0
  embed z: Z = Z
  new create() => None

class M
  embed z: Z = Z
  var x: U64 = 0

class N
  var x: U64 = 0
  embed b: B = B

actor Main
  embed z: Z = Z
  embed b: B = B

  new create(env: Env) =>
    let c = C
    let s = S
    let p = P
    let m = M
    let n = N
    if ((digestof c.z) != (digestof c)) or
      ((digestof s.z) != (digestof s)) or
      ((digestof p.z) != (digestof p)) or
      ((digestof m.z) != (digestof m)) or
      ((digestof n.b) != (digestof n)) or
      ((digestof n.b.z) != (digestof n)) or
      ((digestof z) != (digestof this)) or
      ((digestof b) != (digestof this))
    then
      env.exitcode(1)
    end
