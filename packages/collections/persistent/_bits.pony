primitive _Bits
  fun set_bit(bm: U32, i: U32): U32 =>
    bm or (U32(1) <<~ i)

  fun clear_bit(bm: U32, i: U32): U32 =>
    bm and (not (U32(1) <<~ i))

  fun has_bit(bm: U32, i: U32): Bool =>
    (bm and (U32(1) <<~ i)) != 0

  fun mask(hash: U32, l: U8): U32 =>
    (hash >> (U32(5) *~ l.u32_unsafe())) and 0x01f
