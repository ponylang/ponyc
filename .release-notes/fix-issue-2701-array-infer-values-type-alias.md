## Fix compiler crash when passing an array literal to `OutStream.writev`

Previously, the compiler would crash with an assertion failure when an array literal was passed to `env.out.writev` (or any other call expecting a `ByteSeqIter`):

```pony
actor Main
  new create(env: Env) =>
    env.out.writev([])
    env.out.writev(["foo"; "bar"])
```

The compiler's element-type inference for array literals, when the antecedent was an interface whose `values` method returned a type alias (such as `ByteSeqIter`, where `ByteSeq` is `(String | Array[U8] val)`), failed to fully strip viewpoint arrows from the inferred type. The leftover arrows eventually reached code generation and triggered an internal assertion.

This has been fixed. Code of the shape above now compiles and runs correctly.
