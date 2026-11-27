// A declares f with no body and B gives f a body with camelCase locals; C
// provides both, so its f takes B's body. Each local must keep its own name in
// C's copy, so the test expects 1 + 2 + ... + 6, printing 21.
trait A
  fun f(): U32
trait B
  fun f(): U32 =>
    let aOne: U32 = 1
    let aTwo: U32 = 2
    let aThree: U32 = 3
    let aFour: U32 = 4
    let aFive: U32 = 5
    let aSix: U32 = 6
    aOne + aTwo + aThree + aFour + aFive + aSix
class C is (A & B)
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f().string())
