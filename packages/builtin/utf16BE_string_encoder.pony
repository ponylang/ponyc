primitive UTF16BEStringEncoder is StringEncoder

  fun encode(value: U32): (USize, U32) =>
    if value < 0xD800 then
      return (2, _reverse_bytes(value))
    elseif value < 0xE000 then
      return (2, 0xFDFF) // These are not legal unicode codepoints
    elseif value < 0x10000 then
      return (2, _reverse_bytes(value))
    elseif value < 0x200000 then
      let value' = value - 0x10000
      return (4, _reverse_bytes(((value' >> 10) + 0xD800)) + (_reverse_bytes((value' and 0x3FF) + 0xDC00) << 16))
    else
      (2, 0xFDFF) // These are not legal unicode codepoints
    end

  fun tag _reverse_bytes(v: U32): U32 =>
    ((v and 0xFF) << 8) + (v >> 8)

primitive UTF16BEStringDecoder is StringDecoder

  fun decode(b: U32): (U32, U8) =>

    let err: (U32, U8) = (0xFFFD, 2)
    let pair1:U32 = ((b and 0xFFFF0000) >> 16)

    if pair1 < 0xD800 then
      return (pair1, 2)
    elseif pair1 < 0xE000 then
      if (pair1 > 0xDBFF) then
        return err
      end
      let pair2:U32 = b and 0xFFFF
      if (pair2 < 0xDC00) then
        return err
      end
      return ((0x10000 + ((pair1 - 0xD800) << 10) + (pair2 - 0xDC00)), 4)
    else
      return (pair1, 2)
    end
