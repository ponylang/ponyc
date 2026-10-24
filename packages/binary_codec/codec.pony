primitive InsufficientData
  """
  Returned when a decode operation needs more bytes than are available.

  In a streaming context, this means "wait for more data." It is not an
  error in the data itself — the input is simply incomplete.
  """
  fun string(): String iso^ => "Insufficient data".clone()

class val MalformedData
  """
  Returned when a decode operation encounters structurally invalid data.

  Unlike `InsufficientData`, this indicates the data itself is wrong — a
  literal mismatch, an unknown tag, or a value that violates a protocol
  constraint. No amount of additional data will fix it.
  """
  let _message: String
  let _bit_offset: USize

  new val create(message': String, bit_offset': USize = 0) =>
    _message = message'
    _bit_offset = bit_offset'

  fun message(): String => _message
  fun bit_offset(): USize => _bit_offset

  fun string(): String iso^ =>
    (_message + " at bit offset " + _bit_offset.string()).clone()

type DecodeError is (InsufficientData | MalformedData)
  """
  The error union returned by `decode`: either the data ran out
  (`InsufficientData`) or it was structurally wrong (`MalformedData`).
  """

class val EncodeViolation
  """
  Returned when an encode operation fails — typically because a value does
  not fit the codec's constraints (e.g., a U8 value exceeding a 4-bit field)
  or because the output is not byte-aligned.
  """
  let _message: String

  new val create(message': String) =>
    _message = message'

  fun message(): String => _message
  fun string(): String iso^ => _message.clone()

type EncodeError is EncodeViolation
  """
  The error type returned by `encode`.
  """

trait val Codec[A: Any val]
  """
  A bidirectional codec that decodes bytes into a value and encodes a value
  into bytes.

  Codecs are `val` — immutable and shareable across actors. Small codecs
  compose into larger ones via combinators like `Seq2`, `Mapped`, and
  `FlatZip`.

  `decode` takes an array and a bit offset, returning either the decoded
  value paired with the number of bits consumed, or a `DecodeError`. The
  array is `val` so byte-slice codecs like `Bytes` can return zero-copy
  views via `Array.trim`.

  `encode` converts a value to bytes, returning either the encoded bytes or
  an `EncodeError`.
  """
  fun decode(data: Array[U8] val, offset: USize)
    : ((A, USize) | DecodeError)
    """
    Decode a value from `data` starting at the given bit offset.
    """

  fun encode(value: A): (Array[U8] val | EncodeError) =>
    let writer = _BitWriter
    match \exhaustive\ _encode_bits(value, writer)
    | let w: _BitWriter ref =>
      try w.done()?
      else EncodeViolation("Non-byte-aligned output")
      end
    | let e: EncodeError => e
    end

  fun _encode_bits(value: A, writer: _BitWriter ref)
    : (_BitWriter ref | EncodeError)
