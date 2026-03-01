"""
This won't compile if there is a regression for issue #4612.
"""

actor Main
  new create(env: Env) =>
    None

primitive Prim
  fun ignore() => None

trait A
  fun union(): (Prim | None)

  fun match_it(): Bool =>
    match \exhaustive\ union()
    | let p: Prim =>
      p.ignore()
      true
    | None =>
      false
    end

class B is A
  fun union(): Prim =>
    Prim
