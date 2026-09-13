## Add `\by_value\` FFI annotation for struct-by-value passing

Pony's FFI passes all structs behind pointers. Calling a C function that takes or returns a struct by value previously required writing a C wrapper by hand. The `\by_value\` annotation eliminates that — the compiler generates the wrapper for you.

Annotate each parameter or return type that the C function expects by value:

```pony
struct Point
  var x: F64 = 0
  var y: F64 = 0

use @point_add[Point \by_value\](a: Point \by_value\, b: Point \by_value\)
```

Struct fields must be primitives or pointers. Nested embedded structs, zero-field structs, 128-bit integers, and variadic functions are not supported.
