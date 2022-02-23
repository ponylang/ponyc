## Rename `ponytest` package to match standard library naming conventions

We recently realized that when we renamed large portions of the standard library to conform with our naming standards, that we missed the `ponytest` package. To conform with the naming convention, the `ponytest` package as been renamed to `pony_test`.

You'll need to update your test code from:

```pony
use "ponytest"
```

to

```pony
use "pony_test"
```
