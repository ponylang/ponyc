class Pointer[A]
  new create(len: U64) => compiler_intrinsic

  fun ref _realloc(len: U64): Pointer[A] => compiler_intrinsic

  fun box _apply(i: U64): this->A => compiler_intrinsic

  fun ref _update(i: U64, v: A): A^ => compiler_intrinsic

  fun ref _copy(offset: U64, src: Pointer[A] box, len: U64): U64 =>
    compiler_intrinsic

  fun box _concat(len: U64, with: Pointer[A] box, withlen: U64): Pointer[A] iso^
    =>
    compiler_intrinsic

  fun tag u64(): U64 => compiler_intrinsic
