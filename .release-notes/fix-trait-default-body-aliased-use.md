## Fix trait default method bodies failing to compile with aliased use packages

When a trait defined a default method body that referenced a package through an aliased `use` statement, and a type in a different file implemented the trait without its own alias for the same package, the compiler would report "can't access package." The alias lived in the trait's module scope and was not carried along when the default body was copied into the implementing type. Unaliased `use` imports were not affected.

```pony
// greeting.pony
use collections = "collections"

trait Greeting
  fun hello() =>
    // This body is copied into any type that implements Greeting.
    // The reference to `collections` failed when the implementing
    // type was in a different file without its own alias.
    let hi = collections.Map[String, String]
    hi.insert("hello", "world!")
```

```pony
// main.pony — no `use collections` needed here
actor Main is Greeting
  new create(env: Env) =>
    hello()
```
