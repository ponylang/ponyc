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

    // Test 4: Recover block producing a tuple element whose type gets widened.
    // Exercises whether the type widening interacts correctly with AST nodes
    // that were transformed during earlier passes (recover creates scope
    // changes that sugar/refer process before expr).
    let cc4: Change = ((0, 0), "bob", Dependency, (Link,
      recover val
        let s: String = "abc"
        s + "def"
      end))
    if not CheckChange.check(cc4) then
      @fprintf(@pony_os_stdout(), "FAIL: recover in tuple element\n".cstring())
      @exit(1)
    end

    // Test 5: Lambda inside a recover block producing a tuple element.
    // Lambdas are desugared into object literals during the sugar pass and
    // require pass catchup. If the type widening conflicts with the caught-up
    // AST, this will fail at compile time or produce wrong results.
    let cc5: Change = ((0, 0), "bob", Dependency, (Link,
      recover val
        let f = {(s: String): String => s + "!" }
        f("123")
      end))
    if not CheckChange.check(cc5) then
      @fprintf(@pony_os_stdout(), "FAIL: lambda in tuple element\n".cstring())
      @exit(1)
    end

    // Test 6: Object literal producing a tuple element.
    // Object literals, like lambdas, create new type definitions during expr
    // and require pass catchup on the generated AST.
    let cc6: Change = ((0, 0), "bob", Dependency, (Link,
      object val is Stringable
        fun string(): String iso^ => "obj".clone()
      end.string()))
    if not CheckChange.check(cc6) then
      @fprintf(@pony_os_stdout(), "FAIL: object literal in tuple element\n".cstring())
      @exit(1)
    end
