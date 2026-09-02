## Fix compiler crash on `is` with a constructor call

Comparing a freshly constructed object with `is` or `isnt` is an error because the comparison is always false. Two shapes of constructor call crashed the compiler instead of reporting that error: a constructor with its own type parameters, as in `Foo[String].create[U8]("x", U8(1)) is y`, and a constructor called on a value, as in `x.create() is y`. Both now report the error.

A primitive's constructor returns the one instance, so comparing its result with `is` is allowed. That comparison crashed when the constructor had type parameters or was called on a value, and was wrongly rejected when the primitive was named through a type alias. All three now compile.
