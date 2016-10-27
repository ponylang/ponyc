primitive Bool is Stringable
  new create(from: Bool) => from

  fun eq(y: Bool): Bool => this == y
  fun ne(y: Bool): Bool => this != y
  fun op_and(y: Bool): Bool => this and y
  fun op_or(y: Bool): Bool => this or y
  fun op_xor(y: Bool): Bool => this xor y
  fun op_not(): Bool => not this

  fun i8(): I8 => compile_intrinsic
  fun i16(): I16 => compile_intrinsic
  fun i32(): I32 => compile_intrinsic
  fun i64(): I64 => compile_intrinsic
  fun i128(): I128 => compile_intrinsic
  fun ilong(): ILong => compile_intrinsic
  fun isize(): ISize => compile_intrinsic

  fun u8(): U8 => compile_intrinsic
  fun u16(): U16 => compile_intrinsic
  fun u32(): U32 => compile_intrinsic
  fun u64(): U64 => compile_intrinsic
  fun u128(): U128 => compile_intrinsic
  fun ulong(): ULong => compile_intrinsic
  fun usize(): USize => compile_intrinsic

  fun f32(): F32 => compile_intrinsic
  fun f64(): F64 => compile_intrinsic

  fun string(): String iso^ =>
    (if this then "true" else "false" end).string()
