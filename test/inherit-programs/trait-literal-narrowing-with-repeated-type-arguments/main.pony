// A trait literal provides Group[B, F, F, B], so two of Group's parameters each
// get the same argument. check's iftype narrows all four, and one narrowing
// names another parameter. Both instantiations must compile; the program must
// print "6 10".
interface val Scored
  fun score(): U64
interface val Ranked
  fun rank(): U64
primitive S is (Scored & Ranked)
  fun score(): U64 => 1
  fun rank(): U64 => 2
trait val Group[A, C, D, E]
  fun check(a: A, c: C, d: D, e: E): U64 =>
    iftype (A, C, D, E) <: (C, Scored, E, Ranked) then
      a.score() + c.score() + d.rank() + e.rank()
    else
      10
    end
trait M
  fun mk[B: Any val, F: Any val](): Group[B, F, F, B] val => object val is Group[B, F, F, B] end
primitive P is M
actor Main
  new create(env: Env) =>
    env.out.print(P.mk[S, S]().check(S, S, S, S).string() + " " + P.mk[U8, S]().check(1, S, S, 2).string())
