## Fix lambda capture types for fields accessed through non-ref receivers

Capturing a field in a lambda or object literal inside a method with a non-`ref` receiver (most commonly the default `box` receiver of `fun`) produced a spurious type error. The compiler typed the capture with the field's declared capability instead of adapting it through the method's receiver, so a `ref` field captured in a `fun box` method was typed as `ref` when it should have been `box`.

Before this fix, the workaround was to specify the capture type explicitly:

```pony
class Foo
  let _data: Array[U64]
  new create() => _data = Array[U64]

  fun box example() =>
    // This failed: "box is not a subtype of ref"
    {()(_data) => _data.size() }

    // Workaround: specify the type manually
    {()(x: Array[U64] box = _data) => x.size() }
```

Both the explicit capture syntax (`{()(field_name) => ...}`) and implicit captures in object literals are fixed.
