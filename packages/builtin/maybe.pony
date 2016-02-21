struct Maybe[A]
  """
  A Maybe[A] is used to encode a possibly-null type. It should _only_ be used
  for structs that need to be passed to and from the C FFI.

  An optional type for anything that isn't a struct should be encoded as a
  union type, for example (A | None).
  """
  new create(that: A) =>
    """
    This re-encodes the type of `that` from A to Maybe[A], allowing `that` to
    be assigned to a field or variable of type Maybe[A]. It doesn't allocate a
    wrapper object: there is no containing object for `that`.
    """
    compile_intrinsic

  new none() =>
    """
    This returns a null pointer typed as a Maybe[A].
    """
    compile_intrinsic

  fun apply(): this->A ? =>
    """
    This re-encodes the type of `this` from Maybe[A] to A, allowing `this` to
    be assigned to a field of variable of type A. If `this` is a null pointer,
    an error is raised.
    """
    compile_intrinsic

  fun is_none(): Bool =>
    """
    Returns true if `this` is null (ie apply would raise an error).
    """
    compile_intrinsic
