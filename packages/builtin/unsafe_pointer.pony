struct UnsafePointer[A]
  """
  An UnsafePointer[A] is a raw memory pointer whose target is NOT backed by
  Pony's memory-safety guarantees. Use it for pointers that originate outside
  Pony — opaque foreign handles returned from C that are later passed back into
  C (e.g. `FILE*`, `DIR*`, `addrinfo*`), or raw foreign memory whose validity
  the FFI author is asserting by hand.

  Unlike `Pointer[A]`, an `UnsafePointer[A]` must never be adopted in place as
  the backing store of a safe Pony object: the only way to bring its data into
  Pony-managed memory is to copy it (see `String.copy_cstring`, which allocates
  fresh memory and copies). It also should never be exposed in a public
  interface.

  Its API is deliberately a small subset of `Pointer`'s. An `UnsafePointer` is
  only ever held as an opaque handle (created null, null-checked, inspected as
  an address for sentinel comparisons) or read so its contents can be copied
  into Pony-managed memory. It has no allocation, pointer-arithmetic, mutation,
  or array-manipulation methods — those belong to `Pointer`, which backs
  Pony-managed memory.

  Like `Pointer[A]`, it has no descriptor and thus can't be included in a union
  or intersection, or be a subtype of any interface.
  """
  new create() =>
    """
    A null pointer.
    """
    compile_intrinsic

  fun _apply(i: USize): this->A =>
    """
    Retrieve index i. Used by `String.copy_*` to read foreign data on the way
    to copying it into Pony-managed memory.
    """
    compile_intrinsic

  fun _copy_to(that: Pointer[this->A!], n: USize): this->UnsafePointer[A] =>
    """
    Copy n elements from this to that. The destination is a `Pointer` because
    copying is precisely how unsafe data is brought into Pony-managed memory.
    """
    compile_intrinsic

  fun tag usize(): USize =>
    """
    Convert the pointer into an integer, e.g. to compare a returned handle
    against a non-null sentinel.
    """
    compile_intrinsic

  fun tag is_null(): Bool =>
    """
    Return true for a null pointer, false for anything else.
    """
    compile_intrinsic

  fun tag eq(that: UnsafePointer[A] tag): Bool =>
    """
    Return true if this address is that address. Enables matching a held handle
    against another (e.g. dispatching an ASIO event to its owner).
    """
    compile_intrinsic
