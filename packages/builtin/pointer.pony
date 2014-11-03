class Pointer[A]
  // Space for len instances of A.
  new _create(len: U64) => compiler_intrinsic

  // A null pointer.
  new _null() => compiler_intrinsic

  // Keep the array, but reserve space for len instances of A.
  fun ref _realloc(len: U64): Pointer[A] => compiler_intrinsic

  // Retrieve index i.
  fun box _apply(i: U64): this->A => compiler_intrinsic

  // Set index i and return the previous value.
  fun ref _update(i: U64, v: A): A^ => compiler_intrinsic

  // Delete index i, compact the underlying array, and return the value.
  fun ref _delete(i: U64, len: U64): A^ => compiler_intrinsic

  // Copy len instances of A from src to &this[offset]
  fun ref _copy(offset: U64, src: Pointer[A] box, len: U64): U64 =>
    compiler_intrinsic

  // Create a new array that is this + with, length len + withlen.
  fun box _concat(len: U64, with: Pointer[A] box, withlen: U64): Pointer[A] iso^
    =>
    compiler_intrinsic

  fun tag u64(): U64 => compiler_intrinsic
