## Fix FrameDecoder repeating the same MalformedData error on every received() call

`FrameDecoder` in the `binary_codec` package wraps a `Codec` for use with TCP stream framing — it buffers incoming data across `received()` calls and extracts decoded frames. When a codec returned `MalformedData`, the decoder reported the error but kept the malformed bytes in its buffer. Every subsequent `received()` re-decoded the same bad data and reported the same error. No subsequent frame was ever decoded.

The decoder now discards its buffer after reporting `MalformedData`. The next `received()` starts fresh.
