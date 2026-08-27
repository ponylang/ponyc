## Fix false "unreachable code" error when all branches jump away in a None-returning function

Previously, a function returning `None` where every branch of a control construct jumped away (via `return`, `error`, `break`, or `continue`) was rejected with a false "unreachable code" error:

```pony
actor Main
  fun maybe_error(dont_err: Bool = false) ? =>
    if dont_err then
      return
    else
      error
    end

  new create(env: Env) =>
    try maybe_error()? end
```

```
Error:
main.pony:8:5: unreachable code
```

This affected `if`, `iftype`, `while`, `repeat`, `try`, `match`, and `with` constructs. The error pointed at compiler-generated code the user never wrote. This has been fixed.
