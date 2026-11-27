// T.mk returns an object literal whose method matches a captured value with
// one case and no else; C inherits mk. The program must compile and print ok.
// Everything is in one file.
interface Runner
  fun apply(): None

trait T
  fun v(): (U8 | String)

  fun mk(): Runner val =>
    let x = v()
    object val is Runner
      fun apply(): None =>
        match x
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
