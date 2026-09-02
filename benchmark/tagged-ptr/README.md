# Tagged-pointer benchmarks

Measures the performance of tagged-pointer encoding for small machine words (Bool, U8, I8, U16, I16, U32, I32, F32) in union types. Covers box, unbox, match, identity comparison, interface dispatch, array store/load, and a loop over mixed tagged/None arrays. Non-taggable types (U64, F64) are included as a regression baseline.

Built on the `pony_bench` library. Build with the compiler under test and run directly:

```
./build/release/ponyc benchmark/tagged-ptr -o /tmp/bin -b tagged-ptr
/tmp/bin/tagged-ptr
```
