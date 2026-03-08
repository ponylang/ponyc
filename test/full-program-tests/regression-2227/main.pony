"""
Regression test for https://github.com/ponylang/ponyc/issues/2227

Tuple literals assigned to a union-of-tuples type produced incorrect
match results because inner tuple elements were stored inline instead
of being boxed as union values.
"""
use @pony_os_stdout[Pointer[None]]()
use @fprintf[I32](stream: Pointer[None], fmt: Pointer[U8] tag, ...)
use @exit[None](status: I32)

type TimeStamp is (I64, I64)
type Principal is String

primitive NOP
primitive Dependency

primitive Link
primitive Unlink
primitive Spawn

type ID is String

type DependencyOp is ((Link, ID) | (Unlink, ID) | Spawn)

type Change is (
  ( TimeStamp , Principal , NOP        , None           )
  | ( TimeStamp , Principal , Dependency , DependencyOp   )
)

primitive CheckChange
  fun check(cc: Change): Bool =>
    match cc
    | (let t: TimeStamp, let p: Principal, let a: Dependency,
       let v: DependencyOp) =>
      match v
      | (Link, let vw: ID) => true
      | (Unlink, let vw: ID) => false
      | Spawn => false
      end
    else
      false
    end

actor Main
  new create(env: Env) =>
    // Test 1: Tuple literal assigned to union (the bug).
    let cc1: Change = ((0, 0), "bob", Dependency, (Link, "123"))
    if not CheckChange.check(cc1) then
      @fprintf(@pony_os_stdout(), "FAIL: literal tuple\n".cstring())
      @exit(1)
    end

    // Test 2: Tuple constructed via intermediate variable (always worked).
    let vv: DependencyOp = (Link, "456")
    let cc2: Change = ((0, 0), "bob", Dependency, vv)
    if not CheckChange.check(cc2) then
      @fprintf(@pony_os_stdout(), "FAIL: constructed tuple\n".cstring())
      @exit(1)
    end

    // Test 3: Tuple literal passed directly as function argument.
    if not CheckChange.check(((1, 2), "alice", Dependency, (Link, "789"))) then
      @fprintf(@pony_os_stdout(), "FAIL: argument tuple\n".cstring())
      @exit(1)
    end
