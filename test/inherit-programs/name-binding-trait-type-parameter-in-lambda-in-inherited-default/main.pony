// Foo[In]'s default returns a lambda whose parameter has type In, and Sizer
// provides Foo[String]. The test expects "size 5": In in the lambda resolves
// to the trait's parameter, bound to String (#6711).
actor Main
  new create(env: Env) =>
    env.out.print(Sizer.describe()("hello"))

trait Foo[In: Any val]
  fun apply(x: In): USize

  fun describe(): {(In): String} =>
    let self: Foo[In] box = this
    {(x: In)(self): String => "size " + self(x).string()}

class Sizer is Foo[String]
  fun apply(x: String): USize => x.size()
