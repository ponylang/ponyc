// Foo.f's then branch indexes a U8 array literal inside a try. Bar is
// Foo[String], so the else branch runs and the program prints 0.
trait Foo[A]
  fun f(a: A): U8 =>
    try
      iftype A <: U8 then
        [as U8: 1](0)?
      else
        0
      end
    else
      7
    end

class Bar is Foo[String]

actor Main
  new create(env: Env) =>
    env.out.print(Bar.f("x").string())
