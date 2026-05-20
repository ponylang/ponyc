## Fix typecheck assertion failure on loops whose branches all jump away

Previously, defining a loop whose body and else clause both jump away (for example, a `while` with `break` in the body and `return` in the else) inside a function other than `create` triggered an internal compiler assertion:

```pony
actor Main
  fun a() =>
    while true do
      break
    else
      return
    end

  new create(env: Env) =>
    a()
```

```
src/libponyc/pass/expr.c:698: pass_expr: Assertion `errors_get_count(options->check.errors) > 0` failed.
```

This has been fixed. Loops whose branches all jump away now compile correctly.
