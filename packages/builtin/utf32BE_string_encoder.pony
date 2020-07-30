primitive UTF32BEStringEncoder is StringEncoder

  fun encode(value: U32): (USize, U32) =>
    (4, _reverse_bytes(value))

  fun tag _reverse_bytes(v: U32): U32 =>
    ((v and 0xFF) << 24) + ((v and 0xFF00) << 8) + ((v and 0xFF0000) >> 8) + ((v and 0xFF000000) >> 24)

primitive UTF32BEStringDecoder is StringDecoder

  fun decode(b: U32): (U32, U8) =>
    (b, 4)
