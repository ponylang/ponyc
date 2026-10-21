## Fix String.utf32 decoding UTF-8-encoded surrogates instead of rejecting them

`String.utf32` decoded the byte sequences for surrogate code points U+D800 through U+DFFF as valid characters. RFC 3629 forbids these sequences in UTF-8, and the `utf32` docstring promises `(0xFFFD, 1)` for invalid encodings. `runes` inherited the problem through `utf32`.

```pony
let s = recover val String.from_array([0xED; 0xA0; 0x80]) end
s.utf32(0)? // was: (0xD800, 3), now: (0xFFFD, 1)
```
