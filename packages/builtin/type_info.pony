primitive TypeInfo
  """
  Compile-time queries about Pony types.
  """

  fun tag size_of[T](): USize =>
    """
    Returns the size, in bytes, of an instance of `T` as it is laid out in
    memory.

    For machine words such as `U64` or `F32`, this is the size of the
    primitive value (8 and 4 bytes respectively). For tuples, this is the
    size of the unboxed tuple. For classes and actors, this is the size of
    the heap-allocated structure, including the type descriptor pointer
    (and, for actors, the actor bookkeeping fields). For structs, this is
    the C-compatible struct size (no descriptor).

    `T` must be a concrete type. Calling `size_of` with a union,
    intersection, interface, or trait is a compile-time error. Although
    Pony does not normally allow `struct` types as type arguments,
    `TypeInfo.size_of` accepts them as a special case — the type parameter
    is phantom and never used at runtime, so the usual reasons for
    rejecting structs do not apply.
    """
    compile_intrinsic
