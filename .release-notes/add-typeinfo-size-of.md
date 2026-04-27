## Add `TypeInfo.size_of[T]()` for compile-time type-size queries

A new `TypeInfo` primitive has been added to the `builtin` package, exposing a single compile-intrinsic method:

```pony
primitive TypeInfo
  fun tag size_of[T](): USize
```

`TypeInfo.size_of[T]()` returns the in-memory size, in bytes, of an instance of `T`:

- For machine words (`U8`, `U64`, `F32`, ...) it returns the unboxed primitive size, e.g. `size_of[U64]() == 8`.
- For tuples it returns the unboxed tuple size, e.g. `size_of[(U64, U64)]() == 16`.
- For non-machine-word primitives such as `None` it returns the size of the type-descriptor pointer.
- For classes and actors it returns the size of the heap-allocated structure, including the descriptor pointer (and, for actors, the actor bookkeeping fields).
- For structs it returns the C-compatible struct size (no descriptor), including support for embedded and packed structs.

```pony
actor Main
  new create(env: Env) =>
    env.out.print(TypeInfo.size_of[U64]().string())          // 8
    env.out.print(TypeInfo.size_of[(U8, U8, U8)]().string()) // 3
```

`T` must be a concrete type. Calling `size_of` with a union, intersection, interface, or trait is a compile-time error. Although Pony does not normally allow struct types as type arguments, `TypeInfo.size_of` accepts them as a special case so that struct sizes can be queried directly.
