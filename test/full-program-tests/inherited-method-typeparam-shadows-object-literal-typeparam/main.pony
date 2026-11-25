// An object literal in a generic primitive gets the primitive's type
// parameters. Its provided trait's default method declares a type parameter
// with the same name as one of them (`Into`).
use @pony_exitcode[None](code: I32)

trait Convert[In: Any val, Out: Any val]
  fun apply(src: In): Out

  fun into[Into: Any val](from: {(Out): Into} val): Convert[In, Into] =>
    Converter[In, Out, Into](this, from)

primitive Converter[In: Any val, Out: Any val, Into: Any val]
  fun apply(conv: Convert[In, Out] box, from: {(Out): Into} val)
    : Convert[In, Into]
  =>
    object is Convert[In, Into]
      fun apply(src: In): Into =>
        from(conv(src))
    end

class Size is Convert[String, USize]
  fun apply(src: String): USize => src.size()

actor Main
  new create(env: Env) =>
    let doubled = Size.into[USize]({(n: USize): USize => n * 2})
    let tripled = doubled.into[USize]({(n: USize): USize => n * 3})
    if tripled("abcd") != 24 then
      @pony_exitcode(1)
    end
