"""
The crypto package includes a variety of common cryptographic algorithms
and functions often useful in cryptographic code for their use in information
security.
"""

interface HashFn
  """
  Produces a fixed-length byte array based on the input sequence.
  """
  fun tag apply(input: ByteSeq): Array[U8] val

primitive ToHexString
  fun tag apply(bs: Array[U8] val): String =>
    """
    Return the lower-case hexadecimal string representation of the given Array
    of U8.
    """
    let fmt = FormatSettingsInt.set_format(FormatHexSmallBare).set_width(2)
    fmt.fill' = '0'
    let out = recover String(bs.size() * 2) end
    for c in bs.values() do
      out.append(c.string(fmt))
    end
    consume out
