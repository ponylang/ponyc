## Remove unused `separator` parameter from `Path.split`

`Path.split` accepted a `separator` parameter that the implementation ignored — the function always used the platform's path separator regardless of the value passed. The parameter has been removed.

Code that passed a separator explicitly no longer compiles:

```pony
// Before
(let head, let tail) = Path.split(my_path, "/")

// After
(let head, let tail) = Path.split(my_path)
```
