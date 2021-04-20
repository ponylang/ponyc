primitive UTF32LEStringEncoder is StringEncoder

  fun encode(value: U32): (USize, U32) =>
    (4, value)

primitive UTF32LEStringDecoder is StringDecoder

  fun decode(b: U32): (U32, U8) =>
    (((b and 0xFF000000) >> 24) +
     ((b and 0xFF0000) >> 8) +
     ((b and 0xFF00) << 8) +
     ((b and 0xFF) << 24), 4
    )
