"""
The crypto package includes a variety of common cryptographic algorithms
and functions often useful in cryptographic code for information security.
"""

interface HashFn
  """
  Produces a fixed-length byte array based on the input sequence.
  """
  fun apply(input: ByteSeq): Array[U8] val
