// A trait literal marks a match on v()'s (U8 | String) result \exhaustive\ but
// matches only U8. C's v() returns U8, so C's copy would cover every case. The
// test expects the not-exhaustive error, because the trait's text is checked
// with the types written in the trait.
interface Runner
  fun apply(): None

trait T
  fun v(): (U8 | String)

  fun mk(): Runner val =>
    let x = v()
    object val is Runner
      fun apply(): None =>
        match \exhaustive\ x
        | let u: U8 => None
        end
    end

class C is T
  new create() => None
  fun v(): U8 => 1

actor Main
  new create(env: Env) =>
    C.mk()()
    env.out.print("ok")
