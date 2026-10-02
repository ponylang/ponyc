## Add iftype specialization for method overloading on type parameters

Methods on generic types can now have multiple definitions distinguished by `iftype` guards on type parameters. The matching body is selected at reification time, so there is no runtime dispatch cost.

```pony
class Container[A: Any val]
  var _data: A

  new create(data: A) =>
    _data = data

  fun describe(): String =>
    "opaque value"

  fun describe(): String iftype A <: Stringable val =>
    _data.string()
```

When `A` satisfies the guard constraint, the specialization is used. When it does not, the unguarded default is used. A method can have multiple specializations; the first in declaration order whose guard matches is selected.

Guards can use `and` or `or` to combine multiple constraints:

```pony
class Pair[A: Any val, B: Any val]
  fun process(): String =>
    "generic"

  fun process(): String iftype A <: Stringable val and B <: Stringable val =>
    "both stringable"

  fun process(): String iftype A <: Stringable val or A <: Hashable val =>
    "stringable or hashable"
```

Traits and interfaces can declare specializations, and concrete types inherit the entire method group:

```pony
trait Describable[A: Any val]
  fun describe(): String =>
    "unknown"

  fun describe(): String iftype A <: Stringable val =>
    "stringable"

class MyType[A: Any val] is Describable[A]
```

When a concrete type provides its own definition, it overrides the trait's specialization. Interfaces with specializations enforce structural subtyping — concrete types must provide matching specializations.

An `or` guard requires all branches to constrain the same type parameter. An `and` guard can constrain different type parameters. Mixing `and` and `or` in a single guard is not allowed.

Specializations must have the same parameter types, receiver capability, and method kind as the default. The return type of a specialization must be a subtype of the default's return type. A specialization can only be partial (`?`) if the default is partial.
