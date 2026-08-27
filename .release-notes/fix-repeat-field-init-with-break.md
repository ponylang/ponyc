## Fix `repeat` loop field initialization tracking with `break`

The compiler rejected valid programs where a `repeat` loop body initialized a field before an unconditional `break`. The `else` clause was required to also initialize the field, even though the body always did so before exiting. This program now compiles:

```pony
actor Main
  var _s: (String | None)
  new create(env: Env) =>
    repeat
      _s = None
      break
    until true else
      None
    end
```
