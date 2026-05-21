// Regression test: an actor's behavior used to satisfy an interface method
// declared with a `box` receiver capability. (The `ref` case — the other
// non-tag receiver cap a behavior can satisfy — is covered by the sibling
// test behavior-satisfies-ref-interface-method.)
//
// A `be` is `fun tag`, and box/ref are subcaps of tag, so the contravariant
// receiver check makes `let x: IFunBox = this` a valid subtype relationship.
// The reach pass, however, keyed the behavior's concrete entry under the
// interface method's cap ("box_apply") via add_rmethod_to_subtype, while the
// later forwarding lookup in reach_method normalizes a behavior's cap to tag
// and searched for "tag_apply". The lookup missed and genfun_forward asserted
// on the NULL result, crashing the compiler. add_rmethod now mirrors
// reach_method's cap normalization on insert, so a behavior's concrete entry
// is always keyed under tag. See ponylang/ponyc#5191.
//
// The behavior sets the exit code, so a missed dispatch — or one carrying the
// wrong argument — fails the test rather than only a compiler crash being
// caught.

use @pony_exitcode[None](code: I32)

interface IFunBox
  fun box apply(s: String)

actor Main
  new create(env: Env) =>
    let x: IFunBox = this
    x("hello")

  be apply(s: String) =>
    if s == "hello" then
      @pony_exitcode(1)
    end
