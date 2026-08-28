## Fix overly strict consumed-variable check in try/else blocks

Previously, consuming a variable after the last error point in a `try` block made the variable unusable in the `else` block, even though the consume could not have executed on any path that reaches `else`:

```pony
actor Main
  new create(env: Env) =>
    try
      partial()?
      consume env  // only runs if partial() succeeded
    else
      consume env  // error: can't use a consumed local
    end

  fun partial() ? => error
```

The example above now compiles correctly.

Variables consumed before or at an error point are still correctly rejected in `else`.
