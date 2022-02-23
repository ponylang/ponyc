## Rename `ponybench` package to match standard library naming conventions

We recently realized that when we renamed large portions of the standard library to conform with our naming standards, that we missed the `ponybench` package. To conform with the naming convention, the `ponybench` package as been renamed to `pony_bench`.

You'll need to update your test code from:

```pony
use "ponybench"
```

to

```pony
use "pony_bench"
```
