trait val StringEncoder
  """
  An Encoder converts unicode codepoints into a variable number of bytes.
  """

  new val create()

  fun encode(value: U32): (USize, U32)
  """
  Convert a codepoint into up to 4 bytes. The first value in the returned tuple indicates the number of
  bytes required for the encoding. The second value contains the encode bytes packed in a U32.
  """
