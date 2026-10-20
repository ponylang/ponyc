## Fix Format.int producing invalid UTF-8 with multi-byte fill characters

`Format.int` with a fill character outside the ASCII range produced invalid UTF-8. The formatter builds the digit string in reverse, then reverses it byte by byte — multi-byte fill characters had their UTF-8 bytes reversed along with the digits.

```pony
// U+00E9 is C3 A9 in UTF-8
Format.int[U8](0 where width = 2, fill = 0xE9)
// was: A9 C3 30 (invalid UTF-8)
// now: C3 A9 30 (valid UTF-8: "é0")
```

`Format` on strings with the same fill was unaffected.
