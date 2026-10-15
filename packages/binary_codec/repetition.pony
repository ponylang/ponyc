class val Count[A: Any val] is Codec[Array[A] val]
  """
  Read a count field, then decode that many instances of the body codec.

  On encode, write the array length as the count, then encode each element.
  The count field is structural — it does not appear in the decoded type.
  """
  let _count_codec: Codec[U16]
  let _body_codec: Codec[A]

  new val create(count_codec: Codec[U16], body_codec: Codec[A]) =>
    _count_codec = count_codec
    _body_codec = body_codec

  fun decode(data: Array[U8] val, offset: USize)
    : ((Array[A] val, USize) | DecodeError)
  =>
    """
    Decode a count followed by that many elements.
    """
    match \exhaustive\ _count_codec.decode(data, offset)
    | (let count: U16, let cc: USize) =>
      let n = count.usize()
      var consumed: USize = cc
      let results = recover iso Array[A](n) end
      var i: USize = 0
      while i < n do
        match \exhaustive\ _body_codec.decode(data, offset + consumed)
        | (let v: A, let cv: USize) =>
          results.push(v)
          consumed = consumed + cv
        | let e: DecodeError => return e
        end
        i = i + 1
      end
      (consume results, consumed)
    | let e: DecodeError => e
    end

  fun _encode_bits(value: Array[A] val, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    match \exhaustive\ _count_codec._encode_bits(value.size().u16(), writer)
    | let w: _BitWriter ref =>
      var w' = w
      for item in value.values() do
        match \exhaustive\ _body_codec._encode_bits(item, w')
        | let w2: _BitWriter ref => w' = w2
        | let e: EncodeError => return e
        end
      end
      w'
    | let e: EncodeError => e
    end

class val Many[A: Any val] is Codec[Array[A] val]
  """
  Decode elements repeatedly until the input is exhausted.

  Stops on `InsufficientData` (not enough bytes for another element).
  Propagates `MalformedData` as an error — a corrupt element stops the
  parse rather than being silently treated as "incomplete."

  On encode, encodes each element in sequence.
  """
  let _codec: Codec[A]

  new val create(codec: Codec[A]) =>
    _codec = codec

  fun decode(data: Array[U8] val, offset: USize)
    : ((Array[A] val, USize) | DecodeError)
  =>
    """
    Decode elements until `InsufficientData` signals no more fit.
    """
    var consumed: USize = 0
    let results = recover iso Array[A] end
    var continue' = true
    while continue' do
      match \exhaustive\ _codec.decode(data, offset + consumed)
      | (let v: A, let cv: USize) =>
        results.push(v)
        consumed = consumed + cv
      | InsufficientData => continue' = false
      | let m: MalformedData => return m
      end
    end
    (consume results, consumed)

  fun _encode_bits(value: Array[A] val, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    var w = writer
    for item in value.values() do
      match \exhaustive\ _codec._encode_bits(item, w)
      | let w2: _BitWriter ref => w = w2
      | let e: EncodeError => return e
      end
    end
    w
