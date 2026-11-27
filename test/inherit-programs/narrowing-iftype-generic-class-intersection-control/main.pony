// A generic class written by hand with the shape an inherited copy would have:
// mk returns (B2 & A) and f uses iftype A <: Stringable. Q[U8, (U8 | None)]
// must print 7 and the Array[U8] version must print "other".
class Q[B2: Any val, A: Any val]
  let _b: (B2 & A)
  new create(b: (B2 & A)) => _b = b
  fun mk(): (B2 & A) => _b
  fun f(): String =>
    iftype A <: Stringable val then mk().string() else "other" end

actor Main
  new create(env: Env) =>
    env.out.print(Q[U8, (U8 | None)](U8(7)).f())
    env.out.print(Q[Array[U8] val, (Array[U8] val | None)]([as U8: 1]).f())
