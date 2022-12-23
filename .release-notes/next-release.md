## Fix crash when handling parameters with invalid types

Fixes a compiler assert when checking a parameter for autorecovery that has an invalid type.

## Fix some infinite loops during typechecking

Some recursive constraints could cause the typechecker to go into
an infinite loop while trying to check subtyping. These infinite
loops have now been fixed. An example program that demonstrates
the issue is below:

```
primitive Foo[T: (Unsigned & UnsignedInteger[T])]
  fun boom() =>
    iftype T <: U8 then
      None
    end

actor Main
  new create(env: Env) =>
    Foo[U8].boom()
```

The use of T: UnsignedInteger[T] is valid and expected, but wasn't
being handled correctly in this situation by the typechecker in this context.

