// An embedded struct with no fields has the address of the object that holds
// it, wherever it is declared, so its address is always inside that object.
struct Z
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

actor Main
  embed z: Z = Z

  new create(env: Env) =>
    let c = C
    let s = S
    let p = P
    let m = M
    if ((digestof c.z) != (digestof c)) or
      ((digestof s.z) != (digestof s)) or
      ((digestof p.z) != (digestof p)) or
      ((digestof m.z) != (digestof m)) or
      ((digestof z) != (digestof this))
    then
      env.exitcode(1)
    end
