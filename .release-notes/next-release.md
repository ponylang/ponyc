## Bound PonyTest concurrent execution to scheduler thread count

PonyTest previously launched all non-exclusive tests at once with no concurrency limit. In large test suites this overwhelmed the scheduler, causing spurious timeouts in tests that pass when run on their own.

Concurrent test execution is now bounded by the number of scheduler threads. Tests beyond that limit are queued and started as earlier tests complete. Small test suites are unaffected — when there are fewer tests than threads, all tests still start immediately.

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

