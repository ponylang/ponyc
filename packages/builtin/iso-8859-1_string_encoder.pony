primitive ISO88591StringEncoder is StringEncoder

  fun encode(value: U32): (USize, U32) =>
    if value < 0x100 then
      return (1, value)
    else
      return (1, 0x3F)
    end

primitive ISO88591StringDecoder is StringDecoder

  fun decode(b: U32): (U32, U8) =>
    (((b and 0xFF000000) >> 24), 1)
