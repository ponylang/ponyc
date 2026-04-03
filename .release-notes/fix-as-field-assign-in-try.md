## Fix spurious error when assigning to a field on an `as` cast in a try block

Assigning to a field on the result of an `as` expression inside a `try` block incorrectly produced an error about consumed identifiers:

```pony
class Wumpus
  var hunger: USize = 0

actor Main
  new create(env: Env) =>
    let a: (Wumpus | None) = Wumpus
    try
      (a as Wumpus).hunger = 1
    end
```

```
can't reassign to a consumed identifier in a try expression if there is a
partial call involved
```

The workaround was to use a `match` expression instead. This has been fixed. The `as` form now compiles correctly, including when chaining method calls before the field assignment (e.g., `(a as Wumpus).some_method().hunger = 1`).
