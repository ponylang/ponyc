## Fix VariableSize body codec seeing the full buffer instead of the declared size

`VariableSize` in `binary_codec` passed the entire remaining buffer to its body codec instead of limiting it to the declared size. Body codecs that read to the end of their input (`BytesRest`, `Many`) consumed bytes beyond the declared size. A `VariableSize` followed by another field produced `MalformedData` instead of decoding both fields, and `FrameDecoder` could not extract multiple length-prefixed frames from a single chunk. A buffer with a complete size field but an incomplete body also produced `MalformedData` instead of `InsufficientData`.

The body codec now decodes from a view trimmed to the declared size.
