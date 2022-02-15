## Fix incorrect "field not initialized" error with try/else

Previously, due to a bug in the compiler's try/else handling code, the following code would be rejected as not initializing all object fields:

```pony
actor Main
  var _s: (String | None)

  new create(env: Env) =>
    try
      _s = "".usize()?.string()
    else
      _s = None
    end
```
