## Add `\c_api\` annotation for C-ABI interop (experimental)

This feature is experimental and may change in future releases.

The `\c_api\` annotation on a class, primitive, struct, or actor exposes its public methods to C callers. The compiler generates C-ABI wrapper functions and a `.h` header that C shim files can include.

```pony
class \c_api\ val Adder
  let _base: I64
  new val create(base: I64) => _base = base
  fun val add(x: I64): I64 => _base + x
```

The `use` alias in the consuming package determines C-facing names. With `use "mylib"`, the wrapper is `mylib_Adder_add` and the header is `mylib_export.h`. With `use math = "mylib"`, they become `math_Adder_add` and `math_export.h`. In the main package, names have no prefix.

```c
#include "mylib_export.h"

int64_t add_from_c(void* adder, int64_t x) {
  return mylib_Adder_add(adder, x);
}
```

Exported primitive methods omit the `self` parameter — primitives are stateless, so the C caller doesn't need to pass a receiver.

To export a concrete reification of a generic type, annotate a type alias:

```pony
class MyBox[A]
  let _value: A
  new val create(value: A) => _value = value
  fun val get(): A => _value

type \c_api\ BoxedI64 is MyBox[I64]
```

Constructors, behaviors, private methods, partial methods, and methods with tuple parameters or return types are excluded from export. It is an error to annotate a type whose methods are all excluded. Generic types cannot be exported directly; use a type alias to export a concrete reification.
