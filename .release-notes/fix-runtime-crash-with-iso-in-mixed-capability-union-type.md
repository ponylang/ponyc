## Fix runtime crash with iso in mixed-capability union type

Matching on a destructively read `iso` field would crash at runtime with a segfault in the GC when the field's union type also contained a `val` member. For example, this program would crash:

```pony
class A
  var v: Any val
  new create(v': Any val) =>
    v = consume v'

class B

actor Foo
  var _x: (A iso | B val | None)

  new create(x': (A iso | B val | None)) =>
    _x = consume x'

  be f() =>
    match (_x = None)
    | let a: A iso => None
    end

actor Main
  new create(env: Env) =>
    let f = Foo(recover A("hello".string()) end)
    f.f()
```

The crash occurred because the GC traced the `iso` object incorrectly, leading to a reference count imbalance and use-after-free. This has been fixed.
