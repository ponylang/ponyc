class val Mapped[A: Any val, B: Any val] is Codec[B]
  """
  Bidirectional type mapping between a wire type `A` and a domain type `B`.

  The forward function transforms decoded `A` values into `B` values, and
  may fail (partial) to signal malformed data (e.g., checksum validation).
  The reverse function transforms `B` back to `A` for encoding.
  """
  let _codec: Codec[A]
  let _forward: {(A): B ?} val
  let _reverse: {(B): A} val

  new val create(
    c: Codec[A],
    f: {(A): B ?} val,
    r: {(B): A} val)
  =>
    _codec = c
    _forward = f
    _reverse = r

  fun decode(data: Array[U8] val, offset: USize)
    : ((B, USize) | DecodeError)
  =>
    match \exhaustive\ _codec.decode(data, offset)
    | (let va: A, let consumed: USize) =>
      try
        (_forward(va)?, consumed)
      else
        MalformedData("Mapped forward mapping failed", offset)
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: B, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    _codec._encode_bits(_reverse(value), writer)

class val Widen[V: Any val, Msg: Any val] is Codec[Msg]
  """
  Widen a codec's type from a specific variant `V` to a broader union `Msg`.

  On decode, the narrower value is returned as `Msg` (since `V` is a
  subtype of `Msg`). On encode, the narrowing function attempts to cast
  `Msg` back to `V`; if the cast fails, encoding errors.
  """
  let _codec: Codec[V]
  let _narrow: {(Msg): V ?} val

  new val create(codec: Codec[V], narrow: {(Msg): V ?} val) =>
    _codec = codec
    _narrow = narrow

  fun decode(data: Array[U8] val, offset: USize)
    : ((Msg, USize) | DecodeError)
  =>
    match \exhaustive\ _codec.decode(data, offset)
    | (let v: V, let consumed: USize) =>
      try ((v as Msg), consumed)
      else MalformedData("Widen cast failed", offset)
      end
    | let e: DecodeError => e
    end

  fun _encode_bits(value: Msg, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
  =>
    try
      _codec._encode_bits(_narrow(value)?, writer)
    else
      EncodeViolation("Value does not match this variant")
    end
