// Regression test for ponylang/ponyc#5859.
//
// A trait pattern accepted by the loosening at compile time must be
// reachable at codegen — if the reified pattern type is not otherwise
// instantiated in the program, matching still has to compile.
// T[Wrap[U32] val] appears only in the pattern here; the program
// compiles and runs. The match itself falls through (A reifies to U8,
// not Wrap[U32] val) — exit 3.

use @pony_exitcode[None](code: I32)

class val Wrap[A: Any #share]
  let value: A
  new val create(v: A) => value = v

trait val T[A: Any #share]
  fun get_value(): A

class val Cons[A: Any #share] is T[A]
  let _v: A
  new val create(v: A) => _v = v
  fun get_value(): A => _v

primitive Check
  fun apply[A: Any #share](x: Cons[A]): I32 =>
    match x
    | let _: T[Wrap[U32] val] val => 1
    else
      3
    end

actor Main
  new create(env: Env) =>
    let c: Cons[U8] = Cons[U8](7)
    @pony_exitcode(Check[U8](c))
