// Combined's type parameter B is passed to Group as A, and Group's check
// method has its own type parameter B.
use @pony_exitcode[None](code: I32)

interface val Labeled
  fun label(): U64

primitive Label is Labeled
  fun label(): U64 => 20

trait Group[A: Any val]
  fun check[B: Labeled](arg: A, item: B): U64 => item.label()

primitive Combined[B: Any val] is Group[B]

actor Main
  new create(env: Env) =>
    if Combined[None].check[Label](None, Label) != 20 then
      @pony_exitcode(1)
    end
