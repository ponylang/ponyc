## Fix binary_codec silently dropping fields at non-byte-aligned positions

Encoding a byte-aligned codec (`U16LE`, `U32LE`, `U64LE`, `U128LE`, `Bytes`, `BytesRest`, `Literal`, or `VariableSize`) after a sub-byte field silently produced output with the byte-aligned field missing. A `Seq` whose first element was `BitsU8(4)` and second was `U16LE` encoded only the 4-bit prefix; the 16-bit value was dropped with no error.

Decoding the same codecs at a non-byte-aligned offset returned `MalformedData`, even when the data was present. `U8Codec`, `I8Codec`, and `U128BE` had the same decode restriction despite their encoders already working at any offset.

All codecs now encode and decode at any bit offset.
