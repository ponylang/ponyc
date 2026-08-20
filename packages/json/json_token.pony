type JSONToken is
  ( JSONTokenObjectStart
  | JSONTokenObjectEnd
  | JSONTokenArrayStart
  | JSONTokenArrayEnd
  | JSONTokenTrue
  | JSONTokenFalse
  | JSONTokenNull
  | JSONTokenKey
  | JSONTokenString
  | JSONTokenNumber
  )
  """
  A single token emitted by `JSONTokenParser`.

  The structural tokens (`{` `}` `[` `]`) and the literals (`true`, `false`,
  `null`) are primitives — the type is the whole value. The three tokens that
  carry data — a key, a string, a number — are classes whose `value` field holds
  it, so a token is a value you can hold: match it and read `value`, with nothing
  shared or overwritten between tokens.

  A key or string value that fits within a single fed chunk with no escapes is a
  zero-copy view into that chunk. Holding such a token (or its value) keeps the
  chunk's bytes in memory until you drop it — extracting one small field from a
  large chunk and holding it pins the whole chunk. A view is also not
  NUL-terminated, so passing its `cpointer()` to C that expects a C string reads
  past the value; use it as a Pony `String`. Strings with escapes, or split across
  a chunk boundary, are decoded copies that hold nothing and are NUL-terminated.

  ```pony
  match token
  | let k: JSONTokenKey    => // k.value : String
  | let s: JSONTokenString => // s.value : String
  | let n: JSONTokenNumber => // n.value : (I64 | F64)
  | JSONTokenObjectStart   => // ...
  end
  ```
  """

primitive JSONTokenObjectStart
  """
  Start of a JSON object (`{`).
  """

primitive JSONTokenObjectEnd
  """
  End of a JSON object (`}`).
  """

primitive JSONTokenArrayStart
  """
  Start of a JSON array (`[`).
  """

primitive JSONTokenArrayEnd
  """
  End of a JSON array (`]`).
  """

primitive JSONTokenTrue
  """
  JSON `true` literal.
  """

primitive JSONTokenFalse
  """
  JSON `false` literal.
  """

primitive JSONTokenNull
  """
  JSON `null` literal.
  """

class val JSONTokenKey
  """
  A JSON object member name. `value` is the decoded key string.
  """
  let value: String
    """
    The decoded key string.
    """

  new val create(value': String) =>
    value = value'

class val JSONTokenString
  """
  A JSON string value. `value` is the decoded string.
  """
  let value: String
    """
    The decoded string value.
    """

  new val create(value': String) =>
    value = value'

class val JSONTokenNumber
  """
  A JSON number. `value` is an `I64` for an integer of 18 or fewer digits,
  otherwise an `F64` — so a very large integer becomes an `F64` even when
  it would have fit in an `I64`.
  """
  let value: (I64 | F64)
    """`I64` for an integer of 18 or fewer digits, otherwise `F64`."""

  new val create(value': (I64 | F64)) =>
    value = value'
