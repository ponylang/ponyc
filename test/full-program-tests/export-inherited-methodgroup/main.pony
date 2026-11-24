use @call_i64[I64](value: I64)
use @call_u64[U64](value: U64)
use @check_selection[Bool]()

trait Group[A: Any val]
  fun identity(value: A): A => value
  fun identity(value: A): A iftype A <: I64 => value

  fun choice(): Any val => None
  fun choice(): I64 iftype A <: I64 => 42

primitive Combined[B: Any val] is Group[B]

type \c_api\ BoxedI64 is Combined[I64]
type \c_api\ BoxedU64 is Combined[U64]

actor Main
  new create(env: Env) =>
    if @call_i64(10) != 10 then
      env.exitcode(1)
    elseif @call_u64(10) != 10 then
      env.exitcode(2)
    elseif not @check_selection() then
      env.exitcode(3)
    end
