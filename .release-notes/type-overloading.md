## Add type overloading for methods with different parameter types

Methods on a class, actor, or primitive can now have multiple definitions with the same name when their parameter types differ. At each call site, the overload whose parameter types match the arguments is used.

```pony
class Timestamp
  var _epoch: U64

  new create(value: U64) =>
    _epoch = value

  new create(s: String) =>
    _epoch = try s.u64()? else 0 end

  fun get_epoch(): U64 => _epoch

actor Main
  new create(env: Env) =>
    let t1 = Timestamp(U64(100))
    let t2 = Timestamp("200")
    env.out.print(t1.get_epoch().string())
    env.out.print(t2.get_epoch().string())
```

Overloads can also differ by arity:

```pony
class Greeter
  fun apply(): String => "hello"
  fun apply(name: String): String => "hello, " + name
```

Each overload must differ from every other overload in at least one parameter position by nominal type. Overloads that differ only in capability, type parameters, or return type are rejected.

Method chaining (`.>`) works with overloaded methods. Partial application (`~`) is not supported on overloaded methods.
