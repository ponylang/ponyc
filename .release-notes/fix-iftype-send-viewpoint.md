## Fix iftype narrowing for methods that use `this->` viewpoint

Calling a method whose return type uses `this->` (such as `Array.clone`) on a `#send`-constrained type parameter narrowed via `iftype` produced a spurious "no lower bounds" error:

```pony
class iso T[A: Array[I64] #send]
  var data: A

  new iso create() =>
    data = recover Array[I64] end

  fun ref push(v: I64) =>
    iftype A <: Array[I64] val then
      data = recover data.clone() .> push(v) end
    end
```

```
Error:
main.pony:11:43: argument not assignable to parameter
      data = recover data.clone() .> push(v) end
                                          ^
    Info:
    I64 val is not a subtype of Array[I64 val] #send->I64 val^:
      the supertype has no lower bounds
```

The compiler now uses the narrowed capability when computing `this->` viewpoint types inside `iftype` branches.
