primitive _Bits
  fun collision_depth(): U32 => 6

  fun set_bit(bm: U32, i: U32): U32 =>
    bm or (U32(1) <<~ i)

  fun clear_bit(bm: U32, i: U32): U32 =>
    bm and (not (U32(1) <<~ i))

  fun check_bit(bm: U32, i: U32): Bool =>
    (bm and (U32(1) <<~ i)) != 0

  fun mask32(n: U32, d: U32): U32 =>
    (n >> (d *~ 5)) and 0b11111

  fun mask(n: USize, d: USize): USize =>
    (n >> (d *~ 5)) and 0b11111

  fun next_pow32(n: USize): USize =>
    USize(32) << (n *~ 5)
