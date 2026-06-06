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

  Like `Pointer[A]`, it has no descriptor and thus can't be included in a union
  or intersection, or be a subtype of any interface. Most functions on an
  `UnsafePointer[A]` are private to maintain memory safety.
  """
  new create() =>
    """
    A null pointer.
    """
    compile_intrinsic

  new _alloc(len: USize) =>
    """
    Space for len instances of A.
    """
    compile_intrinsic

  fun ref _realloc(len: USize, copy: USize): UnsafePointer[A] =>
    """
    Keep the contents, but reserve space for len instances of A.
    """
    compile_intrinsic

  fun tag _unsafe(): UnsafePointer[A] ref =>
    """
    Unsafe change in reference capability.
    """
    compile_intrinsic

  fun _convert[B](): this->UnsafePointer[B] =>
    """
    Convert from UnsafePointer[A] to UnsafePointer[B].
    """
    compile_intrinsic

  fun _apply(i: USize): this->A =>
    """
    Retrieve index i.
    """
    compile_intrinsic

  fun ref _update(i: USize, value: A!): A^ =>
    """
    Set index i and return the previous value.
    """
    compile_intrinsic

  fun _offset(n: USize): this->UnsafePointer[A] =>
    """
    Return a pointer to the n-th element.
    """
    compile_intrinsic

  fun tag offset(n: USize): UnsafePointer[A] tag =>
    """
    Return a tag pointer to the n-th element.
    """
     _unsafe()._offset(n)

  fun tag _element_size(): USize =>
    """
    Return the size of a single element in an array of type A.
    """
    compile_intrinsic

  fun ref _insert(n: USize, len: USize): UnsafePointer[A] =>
    """
    Creates space for n new elements at the head, moving following elements.
    The array length before this should be len, and the available space should
    be at least n + len.
    """
    compile_intrinsic

  fun ref _delete(n: USize, len: USize): A^ =>
    """
    Delete n elements from the head of pointer, compact remaining elements of
    the underlying array. The array length before this should be n + len.
    Returns the first deleted element.
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
    Convert the pointer into an integer.
    """
    compile_intrinsic

  fun tag is_null(): Bool =>
    """
    Return true for a null pointer, false for anything else.
    """
    compile_intrinsic

  fun tag eq(that: UnsafePointer[A] tag): Bool =>
    """
    Return true if this address is that address.
    """
    compile_intrinsic

  fun tag lt(that: UnsafePointer[A] tag): Bool =>
    """
    Return true if this address is less than that address.
    """
    compile_intrinsic

  fun tag ne(that: UnsafePointer[A] tag): Bool => not eq(that)
  fun tag le(that: UnsafePointer[A] tag): Bool => lt(that) or eq(that)
  fun tag ge(that: UnsafePointer[A] tag): Bool => not lt(that)
  fun tag gt(that: UnsafePointer[A] tag): Bool => not le(that)

  fun tag hash(): USize =>
    """
    Returns a hash of the address.
    """
    usize().hash()

  fun tag hash64(): U64 =>
    """
    Returns a 64-bit hash of the address.
    """
    usize().hash64()
