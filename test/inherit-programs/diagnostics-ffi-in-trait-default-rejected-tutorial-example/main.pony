// The tutorial's example of an FFI call in a trait default: Foo's apply()
// calls @printf. The test expects "Can't call an FFI function in a default
// method or behavior".
use @printf[I32](fmt: Pointer[None] tag, ...)

trait Foo
  fun apply() =>
    // Error: Can't call an FFI function in a default method or behavior
    @printf("Hello from trait Foo\n".cstring())

actor Main is Foo
  new create(env: Env) =>
    this.apply()