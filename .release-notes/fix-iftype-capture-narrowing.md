## Fix lambda and object literal captures inside iftype bodies losing type parameter narrowing

Inside an `iftype` body, capturing a field or local in a lambda or object literal lost the narrowed type constraint. Methods available through the narrowing failed to compile, even though direct references to the same variable worked:

```pony
class Wrapper[A: Any val]
  let _value: A

  fun example() =>
    iftype A <: Stringable val then
      _value.string()                         // worked
      let f = {()(_value) => _value.string()} // "couldn't find 'string' in 'Any val'"
    end
```

The workaround was an explicit intersection annotation on the capture type. All capture forms — bare field, bare local, expression capture, and object literal fields — now preserve the narrowed constraint without annotation.
