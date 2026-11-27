// C.get takes its signature from Sig in sig.pony and its body from Donor in
// donor.pony, so its debug info should name donor.pony, line 9 for the
// method and lines 10-11 for the body. The test checks only that the program
// prints 42.
class C is (Sig & Donor)
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.get().string())
