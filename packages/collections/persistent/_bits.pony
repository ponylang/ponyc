primitive _Bits
  fun set_bit(bm: U32, i: U32): U32 =>
    bm or (U32(1) <<~ i)

  fun clear_bit(bm: U32, i: U32): U32 =>
    bm and (not (U32(1) <<~ i))

  fun has_bit(bm: U32, i: U32): Bool =>
    (bm and (U32(1) <<~ i)) != 0

  fun mask(n: USize, d: USize): USize =>
    (n >> (d *~ 5)) and 0x01f

  fun next_pow32(n: USize): USize =>
    USize(32) << (n *~ 5)
