use "collections"

primitive Base64
  fun encode_pem(data: Bytes box): String iso^ =>
    """
    Encode for PEM (RFC 1421).
    """
    encode(data, '+', '/', '=', 64)

  fun encode_mime(data: Bytes box): String iso^ =>
    """
    Encode for MIME (RFC 2045).
    """
    encode(data, '+', '/', '=', 76)

  fun encode_url(data: Bytes box): String iso^ =>
    """
    Encode for URLs (RFC 4648).
    """
    encode(data, '-', '_')

  fun encode(data: Bytes box, at62: U8 = '+', at63: U8 = '/', pad: U8 = 0,
    linelen: U64 = 0, linesep: String = "\r\n"): String iso^
  =>
    """
    Configurable encoding. The defaults are for RFC 4648.
    """
    let len = ((data.size() + 2) / 3) * 4
    let out = recover String(len) end
    let lineblocks = linelen / 4

    var srclen = data.size()
    var blocks = U64(0)
    var i = U64(0)

    try
      while srclen >= 3 do
        let in1 = data(i)
        let in2 = data(i + 1)
        let in3 = data(i + 2)

        let out1 = in1 >> 2
        let out2 = ((in1 and 0x03) << 4) + (in2 >> 4)
        let out3 = ((in2 and 0x0f) << 2) + (in3 >> 6)
        let out4 = in3 and 0x3f

        out.append_byte(_enc_byte(out1, at62, at63))
        out.append_byte(_enc_byte(out2, at62, at63))
        out.append_byte(_enc_byte(out3, at62, at63))
        out.append_byte(_enc_byte(out4, at62, at63))

        i = i + 3
        blocks = blocks + 1
        srclen = srclen - 3

        if (lineblocks > 0) and (blocks == lineblocks) then
          out.append(linesep)
          blocks = 0
        end
      end

      if srclen >= 1 then
        let in1 = data(i)
        let in2 = if srclen == 2 then data(i + 1) else 0 end

        let out1 = in1 >> 2
        let out2 = ((in1 and 0x03) << 4) + (in2 >> 4)
        let out3 = (in2 and 0x0f) << 2

        out.append_byte(_enc_byte(out1, at62, at63))
        out.append_byte(_enc_byte(out2, at62, at63))

        if srclen == 2 then
          out.append_byte(_enc_byte(out3, at62, at63))
        else
          out.append_byte(pad)
        end

        out.append_byte(pad)
      end

      if lineblocks > 0 then
        out.append(linesep)
      end
    else
      out.truncate(0)
    end

    out

  fun decode(data: Bytes box, at62: U8 = '+', at63: U8 = '/', pad: U8 = '='):
    Array[U8] iso^ ?
  =>
    """
    Configurable decoding. The defaults are for RFC 4648. Missing padding is
    not an error. Non-base64 data, other than whitespace (which can appear at
    any time), is an error.
    """
    let len = (data.size() * 4) / 3
    let out = recover Array[U8](len) end

    var state = U64(0)
    var input = U8(0)
    var output = U8(0)

    for i in Range(0, data.size()) do
      input = data(i)

      let value = match input
      | ' ' | '\t' | '\r' | '\n' => continue
      | pad => break
      | at62 => 62
      | at63 => 63
      | where (input >= 'A') and (input <= 'Z') =>
        (input - 'A')
      | where (input >= 'a') and (input <= 'z') =>
        ((input - 'a') + 26)
      | where (input >= '0') and (input <= '9') =>
        ((input - '0') + 52)
      else
        error
      end

      match state
      | 0 =>
        output = value << 2
        state = 1
      | 1 =>
        out.push(output or (value >> 4))
        output = (value and 0x0f) << 4
        state = 2
      | 2 =>
        out.push(output or (value >> 2))
        output = (value and 0x03) << 6
        state = 3
      | 3 =>
        out.push(output or value)
        state = 0
      else
        error
      end
    end

    match state
    | 1 | 2 => out.push(output)
    end

    out

  fun _enc_byte(i: U8, at62: U8, at63: U8): U8 ? =>
    """
    Encode a single byte.
    """
    match i
    | 62 => at62
    | 63 => at63
    | where i < 26 => 'A' + i
    | where i < 52 => ('a' - 26) + i
    | where i < 62 => ('0' - 52) + i
    else
      error
    end
