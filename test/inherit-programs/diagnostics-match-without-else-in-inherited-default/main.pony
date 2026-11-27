// T.f matches v() with one case and no else. C's v returns U8, so in C the
// match covers every value. The program must compile and print ok.
trait T
  fun v(): (U8 | String)

  fun f(): None =>
    match v()
    | let u: U8 => None
    end

class C is T
  new create() => None
  fun v(): U8 => 1

actor Main
  new create(env: Env) =>
    C.f()
    env.out.print("ok")
