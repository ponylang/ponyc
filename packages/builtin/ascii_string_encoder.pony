primitive ASCIIStringEncoder is StringEncoder

  fun encode(value: U32): (USize, U32) =>
    if value < 0x80 then
      return (1, value)
    else
      return (1, 0x3F)
    end

primitive ASCIIStringDecoder is StringDecoder

  fun decode(b: U32): (U32, U8) =>
    let byte = ((b and 0xFF000000) >> 24)
    if (byte < 0x80) then
      return (byte, 1)
    else
      (0xFFFD, 1)
    end
