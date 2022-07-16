## Allow constructor expressions to be auto-recovered in assignments or arguments

Previously for the following code:

```pony
actor Main
  new create(env: Env) =>
    Bar.take_foo(Foo(88))
    let bar: Foo iso = Foo(88)

class Foo
  new create(v: U8) =>
    None

primitive Bar
  fun take_foo(foo: Foo iso) =>
    None
```

You'd get compilation errors "argument not assignable to parameter" and "right side must be a subtype of left side" for the two lines in `Main.create()`.

We've added checks to see if the constructor expressions can be implicitly auto-recovered, and if they can, no compilation error is generated.

This only applies to cases where the type of the parameter (or the `let` binding) is a simple type, i.e. not a union, intersection or tuple type.

## Support for RISC-V

Pony now supports compiling for RISC-V linux glibc systems. 64 bit `rv64gc`/`lp64d` is tested via CI. In theory 32 bit `rv32gc`/`ilp32d` might work but is untested currently.

