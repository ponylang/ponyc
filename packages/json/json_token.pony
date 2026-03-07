type JsonToken is
  ( JsonTokenNull
  | JsonTokenTrue
  | JsonTokenFalse
  | JsonTokenNumber
  | JsonTokenString
  | JsonTokenKey
  | JsonTokenObjectStart
  | JsonTokenObjectEnd
  | JsonTokenArrayStart
  | JsonTokenArrayEnd
  )

primitive JsonTokenNull
  """JSON null literal."""
primitive JsonTokenTrue
  """JSON true literal."""
primitive JsonTokenFalse
  """JSON false literal."""
primitive JsonTokenNumber
  """After this token, parser.last_number holds the (I64 | F64) value."""
primitive JsonTokenString
  """After this token, parser.last_string holds the string value."""
primitive JsonTokenKey
  """After this token, parser.last_string holds the key name."""
primitive JsonTokenObjectStart
  """Start of a JSON object (`{`)."""
primitive JsonTokenObjectEnd
  """End of a JSON object (`}`)."""
primitive JsonTokenArrayStart
  """Start of a JSON array (`[`)."""
primitive JsonTokenArrayEnd
  """End of a JSON array (`]`)."""
