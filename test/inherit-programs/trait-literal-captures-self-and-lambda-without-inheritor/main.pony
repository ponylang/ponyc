// Convert.into's literal captures this and a lambda over into's own type
// parameter. Nothing provides Convert; the program must still compile and
// print nothing (#6736).
trait Convert[In: Any val, Out: Any val]
  fun apply(src: In): Out

  fun val into[Into: Any val](from: {(Out): Into} val): Convert[In, Into] val =>
    let self: Convert[In, Out] val = this
    object val is Convert[In, Into]
      fun apply(src: In): Into => from(self(src))
    end

actor Main
  new create(env: Env) => None
