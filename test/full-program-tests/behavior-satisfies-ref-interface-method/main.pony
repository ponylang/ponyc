// Regression test: an actor's behavior used to satisfy an interface method
// declared with a `ref` receiver capability. Companion to the sibling test
// behavior-satisfies-box-interface-method — same compiler crash and same fix,
// exercised through the other non-tag receiver cap a behavior can satisfy.
// See that test and ponylang/ponyc#5191 for the mechanism.
//
// The behavior sets the exit code, so a missed dispatch — or one carrying the
// wrong argument — fails the test rather than only a compiler crash being
// caught.

use @pony_exitcode[None](code: I32)

interface IFunRef
  fun ref apply(s: String)

actor Main
  new create(env: Env) =>
    let x: IFunRef = this
    x("hello")

  be apply(s: String) =>
    if s == "hello" then
      @pony_exitcode(1)
    end
