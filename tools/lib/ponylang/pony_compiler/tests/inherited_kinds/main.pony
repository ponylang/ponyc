interface Runner
  fun apply(): U8

trait W
  fun w(): U8 => 7

trait T
  fun v(): (U8 | String)

  fun mk(): Runner val =>
    let x = v()
    object val is (Runner & W)
      fun apply(): U8 =>
        match x
        | let u: U8 => u
        else
          0
        end
    end

class C is T
  new create() => None
  fun v(): U8 => 1

trait G[A: Any val]
  fun g(x: (U8 | String)): U8 => 0
  fun g(x: (U8 | String)): U8 iftype A <: U8 val =>
    match x
    | let u: U8 => u
    else
      0
    end

class D is G[U8]
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.mk()().string())
    env.out.print(D.g(U8(3)).string())
