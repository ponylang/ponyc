// h's object literal reads h's type argument B and its argument b. Calling
// h[U64] on a literal made with B = String must give a literal that takes
// the else branch and prints the U64 value, not the String branch (#6736).
trait U
  fun v(): String => "base"
  fun h[B: Stringable val](b: B): U val =>
    object val is U
      fun v(): String =>
        iftype B <: String then "String:" + b else "other:" + b.string() end
    end

primitive P is U

actor Main
  new create(env: Env) =>
    let t2 = P.h[String]("s")
    env.out.print(t2.v())
    env.out.print(t2.h[U64](0x41414141).v())
