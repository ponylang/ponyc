## Fix incorrect "field not initialized" error with while/else

Previously, due to a bug in the compiler's while/else handling code, the following code would be rejected as not initializing all object fields:

```pony
actor Main
  var _s: (String | None)

  new create(env: Env) =>
    var i: USize = 0
    while i < 5 do
       _s = i.string()
       i = i + 1
    else
      _s = ""
    end
```
