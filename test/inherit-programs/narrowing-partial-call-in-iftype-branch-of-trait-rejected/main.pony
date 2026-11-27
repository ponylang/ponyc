// A trait default calls the partial P.bad() without `?` inside an iftype
// branch, and nothing provides the trait. The test expects both errors: the
// call needs `?`, and f() can raise an error but isn't marked partial.
primitive P
  fun bad(): String ? => error
trait T[A: Any val]
  fun f(): String =>
    iftype A <: U8 then P.bad() else "x" end
actor Main
  new create(env: Env) => env.out.print("ok")
