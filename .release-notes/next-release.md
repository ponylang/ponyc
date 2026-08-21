## Fix default type arguments failing to resolve with aliased use packages

When a generic type had a default type argument that named a type in its own package, using that generic from a consumer that imported the package with an alias (`use m = "pkg"`) failed with "can't find definition of" on the default type. The same code compiled when the package was imported without an alias. This bug was surfaced by lori 0.19.0, which introduced generic TCP types with default backend arguments.

```pony
// In package "tcp":
trait tag Connection[T: Backend ref = RuntimeBackend]

// In the consumer — failed before this fix:
use tcp = "tcp"

actor Main
  let c: tcp.Connection = ...
```

Default type arguments now work correctly regardless of how the package is imported.

