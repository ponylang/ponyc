use @pony_exitcode[None](code: I32)

interface val Scored
  fun score(): I32

interface val Ranked
  fun rank(): I32

primitive Both is (Scored & Ranked)
  fun score(): I32 => 30
  fun rank(): I32 => 20

primitive ScoredOnly is Scored
  fun score(): I32 => 30

primitive RankedOnly is Ranked
  fun rank(): I32 => 20

trait Group[A: Any val, C: Any val]
  fun expression(a: A, c: C): I32 =>
    iftype (A, C) <: (Scored, Ranked) then
      a.score() + c.rank()
    else
      10
    end

  fun specialization(a: A, c: C): I32 => 10

  fun specialization(a: A, c: C): I32 iftype A <: Scored and C <: Ranked =>
    a.score() + c.rank()

primitive Combined[B: Any val] is Group[B, B]

actor Main
  new create(env: Env) =>
    if Combined[Both].expression(Both, Both) != 50 then
      @pony_exitcode(1)
    elseif Combined[ScoredOnly].expression(ScoredOnly, ScoredOnly) != 10 then
      @pony_exitcode(2)
    elseif Combined[RankedOnly].expression(RankedOnly, RankedOnly) != 10 then
      @pony_exitcode(3)
    elseif Combined[Both].specialization(Both, Both) != 50 then
      @pony_exitcode(4)
    elseif Combined[ScoredOnly].specialization(ScoredOnly, ScoredOnly) != 10 then
      @pony_exitcode(5)
    elseif Combined[RankedOnly].specialization(RankedOnly, RankedOnly) != 10 then
      @pony_exitcode(6)
    end
