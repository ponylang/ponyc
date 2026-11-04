class val ThenConst[A: Any val] is Codec[A]
  """
  Decode a value followed by a literal, keeping only the value. On encode,
  write the value followed by the literal.
  """
  let _codec: Codec[A]
  let _literal: Literal

  new val create(codec: Codec[A], literal: Literal) =>
    _codec = codec
    _literal = literal

  fun decode(data: Array[U8] val, offset: USize)
    : ((A, USize) | DecodeError)
  =>
    match \exhaustive\ _codec.decode(data, offset)
    | (let va: A, let ca: USize) =>
      match \exhaustive\ _literal.decode(data, offset + ca)
      | (_, let cl: USize) => (va, ca + cl)
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: A, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _codec._encode_bits(value, writer)
    | let w: _BitWriter ref => _literal._encode_bits(None, w)
    | let e: EncodeError => e
    end

class val ConstThen[A: Any val] is Codec[A]
  """
  Decode a literal followed by a value, keeping only the value. On encode,
  write the literal followed by the value.
  """
  let _literal: Literal
  let _codec: Codec[A]

  new val create(literal: Literal, codec: Codec[A]) =>
    _literal = literal
    _codec = codec

  fun decode(data: Array[U8] val, offset: USize)
    : ((A, USize) | DecodeError)
  =>
    match \exhaustive\ _literal.decode(data, offset)
    | (_, let cl: USize) =>
      match \exhaustive\ _codec.decode(data, offset + cl)
      | (let va: A, let ca: USize) => (va, cl + ca)
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: A, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _literal._encode_bits(None, writer)
    | let w: _BitWriter ref => _codec._encode_bits(value, w)
    | let e: EncodeError => e
    end

class val FlatZip[A: Any val, B: Any val] is Codec[(A, B)]
  """
  Dependent sequencing: the second codec is determined by the first decoded
  value.

  On decode, `A` is decoded first, then `f(A)` produces the codec for `B`.
  On encode, `A` is available from the tuple, so `f(A)` can produce the
  encoder for `B`.
  """
  let _codec_a: Codec[A]
  let _f: {(A): Codec[B]} val

  new val create(ca: Codec[A], f: {(A): Codec[B]} val) =>
    _codec_a = ca
    _f = f

  fun decode(data: Array[U8] val, offset: USize)
    : (((A, B), USize) | DecodeError)
  =>
    match \exhaustive\ _codec_a.decode(data, offset)
    | (let va: A, let ca: USize) =>
      let codec_b = _f(va)
      match \exhaustive\ codec_b.decode(data, offset + ca)
      | (let vb: B, let cb: USize) => ((va, vb), ca + cb)
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: (A, B), writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    let codec_b = _f(value._1)
    match \exhaustive\ _codec_a._encode_bits(value._1, writer)
    | let w: _BitWriter ref => codec_b._encode_bits(value._2, w)
    | let e: EncodeError => e
    end

class val VariableSize[A: Any val] is Codec[A]
  """
  Length-prefixed data: a size codec determines how many bytes the body
  codec reads from. The size field is structural — it does not appear in
  the decoded type.

  On decode: read the size, then decode the body from exactly that many
  bytes. On encode: encode the body, measure it, write the size, then the
  body.
  """
  let _size_codec: Codec[U16]
  let _body_codec: Codec[A]

  new val create(size_codec: Codec[U16], body_codec: Codec[A]) =>
    _size_codec = size_codec
    _body_codec = body_codec

  fun decode(data: Array[U8] val, offset: USize)
    : ((A, USize) | DecodeError)
  =>
    match \exhaustive\ _size_codec.decode(data, offset)
    | (let size: U16, let cs: USize) =>
      match \exhaustive\ _body_codec.decode(data, offset + cs)
      | (let v: A, let cb: USize) =>
        let expected_bits = size.usize() * 8
        if cb != expected_bits then
          MalformedData(
            "VariableSize body consumed " + cb.string() +
              " bits, expected " + expected_bits.string(),
            offset + cs)
        else
          (v, cs + cb)
        end
      | let e: DecodeError => e
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: A, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    let body_writer = _BitWriter
    match \exhaustive\ _body_codec._encode_bits(value, body_writer)
    | let bw: _BitWriter ref =>
      try
        let body_bytes = bw.done()?
        let size = body_bytes.size().u16()
        match \exhaustive\ _size_codec._encode_bits(size, writer)
        | let w: _BitWriter ref =>
          w.write_bytes(body_bytes)
          w
        | let e: EncodeError => e
        end
      else
        EncodeViolation("VariableSize body not byte-aligned")
      end
    | let e: EncodeError => e
    end

class val Conditional[A: Any val] is Codec[(A | None)]
  """
  Conditionally present field: if the flag is true, decode/encode using
  the inner codec; if false, the value is `None`.
  """
  let _flag: Bool
  let _codec: Codec[A]

  new val create(flag: Bool, codec: Codec[A]) =>
    _flag = flag
    _codec = codec

  fun decode(data: Array[U8] val, offset: USize)
    : (((A | None), USize) | DecodeError)
  =>
    if _flag then
      match \exhaustive\ _codec.decode(data, offset)
      | (let v: A, let consumed: USize) => (v, consumed)
      | let e: DecodeError => e
      end
    else
      (None, 0)
    end

  fun _encode_bits(value: (A | None), writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ value
    | let a: A =>
      if not _flag then
        return EncodeViolation("Value present but flag is false")
      end
      _codec._encode_bits(a, writer)
    | None => writer
    end

class val SkipBits is Codec[None]
  """
  Skip a fixed number of bits on decode (reading and discarding them),
  write zero bits on encode.
  """
  let _count: USize

  new val create(count: USize) =>
    _count = count

  fun decode(data: Array[U8] val, offset: USize)
    : ((None, USize) | DecodeError)
  =>
    let cursor = _BitCursor(data, offset / 8, (offset % 8).u8())
    if cursor.available_bits() < _count then return InsufficientData end
    (None, _count)

  fun _encode_bits(value: None, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    writer.write_bits(0, _count.u8())
    writer
