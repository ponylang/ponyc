// Main's `use c = "collections"` names the collections package, so
// c.HashEq[U8].hash(3) equals U8(3).hash() and the program prints true.
use c = "collections"
actor Main
  new create(env: Env) => env.out.print((c.HashEq[U8].hash(3) == U8(3).hash()).string())
