## Fix misleading error for bare lambda captures in tag methods

Capturing a field by name in a lambda inside a `fun tag` method reported the wrong receiver in the error message:

```pony
class Holder
  let _data: String ref
  new create() => _data = String

  fun tag example() =>
    {()(_data) => None }
```

```
can't read a field through {()} ref
```

The error now correctly names the enclosing type:

```
can't read a field through Holder tag
```

The equivalent explicit capture form (`{()(x = _data) => None}`) already reported the correct error. Both forms now produce the same message.
