// An actor larger than an arena (S4 is 8 MiB) sends its embedded String to a
// Reader and has nothing left to do. While the Reader holds the String, the
// actor must not be collected: its memory would be returned to the system.
struct S0
  var f0: U64 = 0
  var f1: U64 = 0
  var f2: U64 = 0
  var f3: U64 = 0
  var f4: U64 = 0
  var f5: U64 = 0
  var f6: U64 = 0
  var f7: U64 = 0
  var f8: U64 = 0
  var f9: U64 = 0
  var f10: U64 = 0
  var f11: U64 = 0
  var f12: U64 = 0
  var f13: U64 = 0
  var f14: U64 = 0
  var f15: U64 = 0
  new create() => None

struct S1
  embed f0: S0 = S0
  embed f1: S0 = S0
  embed f2: S0 = S0
  embed f3: S0 = S0
  embed f4: S0 = S0
  embed f5: S0 = S0
  embed f6: S0 = S0
  embed f7: S0 = S0
  embed f8: S0 = S0
  embed f9: S0 = S0
  embed f10: S0 = S0
  embed f11: S0 = S0
  embed f12: S0 = S0
  embed f13: S0 = S0
  embed f14: S0 = S0
  embed f15: S0 = S0
  new create() => None

struct S2
  embed f0: S1 = S1
  embed f1: S1 = S1
  embed f2: S1 = S1
  embed f3: S1 = S1
  embed f4: S1 = S1
  embed f5: S1 = S1
  embed f6: S1 = S1
  embed f7: S1 = S1
  embed f8: S1 = S1
  embed f9: S1 = S1
  embed f10: S1 = S1
  embed f11: S1 = S1
  embed f12: S1 = S1
  embed f13: S1 = S1
  embed f14: S1 = S1
  embed f15: S1 = S1
  new create() => None

struct S3
  embed f0: S2 = S2
  embed f1: S2 = S2
  embed f2: S2 = S2
  embed f3: S2 = S2
  embed f4: S2 = S2
  embed f5: S2 = S2
  embed f6: S2 = S2
  embed f7: S2 = S2
  embed f8: S2 = S2
  embed f9: S2 = S2
  embed f10: S2 = S2
  embed f11: S2 = S2
  embed f12: S2 = S2
  embed f13: S2 = S2
  embed f14: S2 = S2
  embed f15: S2 = S2
  new create() => None

struct S4
  embed f0: S3 = S3
  embed f1: S3 = S3
  embed f2: S3 = S3
  embed f3: S3 = S3
  embed f4: S3 = S3
  embed f5: S3 = S3
  embed f6: S3 = S3
  embed f7: S3 = S3
  embed f8: S3 = S3
  embed f9: S3 = S3
  embed f10: S3 = S3
  embed f11: S3 = S3
  embed f12: S3 = S3
  embed f13: S3 = S3
  embed f14: S3 = S3
  embed f15: S3 = S3
  new create() => None


actor Holder
  embed big: S4 = S4
  embed big2: S1 = S1
  embed s: String val = String.from_array(recover val
    [as U8: 'h'; 'e'; 'l'; 'l'; 'o'] end)

  new create(r: Reader) =>
    r.take(s)

actor Reader
  let _env: Env
  var _s: (String val | None) = None

  new create(env: Env) =>
    _env = env

  be take(s: String val) =>
    _s = s
    spin(0)

  be spin(i: USize) =>
    if i < 2000 then
      spin(i + 1)
    else
      match _s
      | let s: String val =>
        if s != "hello" then _env.exitcode(1) end
      end
    end

actor Churn
  new create(n: USize) =>
    if n > 0 then
      Churn(n - 1)
    end
    Array[U8].create(1000)

actor Main
  new create(env: Env) =>
    let r = Reader(env)
    var i: USize = 0
    while i < 4 do
      Holder(r)
      i = i + 1
    end
    Churn(1000)
