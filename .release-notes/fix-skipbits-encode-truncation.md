## Fix SkipBits encoding the wrong number of padding bits for counts above 255

`SkipBits` in `binary_codec` truncated its bit count to a `U8` on encode. `SkipBits(264)` encoded 8 zero bits instead of 264, while decode read the full 264 bits. A message containing a `SkipBits` with a count above 255 could not be decoded back from its own encoding.
