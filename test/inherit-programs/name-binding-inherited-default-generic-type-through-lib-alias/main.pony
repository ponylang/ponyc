// lib's default calls c.HashEq[U8] with `use c = "collections"`, and main
// declares `use c` for a package with its own HashEq. The test expects
// "collections 999": the inherited body uses lib's alias, main uses its own.
use "lib"
use c = "mine"
use col = "collections"

class C is T
  new create() => None

actor Main
  new create(env: Env) =>
    let n = C.n()
    let which = if n == col.HashEq[U8].hash(3) then "collections" else n.string() end
    env.out.print(which + " " + c.HashEq[U8].hash(5).string())
