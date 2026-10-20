## Fix Format.int with FormatUTF32 printing decimal digits for U8, U16, I8, and I16

`Format.int` with `FormatUTF32` produced the decimal representation instead of the UTF-32 character for `U8`, `U16`, `I8`, and `I16` values.

```pony
// was: "65"
// now: "A"
Format.int[U8](65, FormatUTF32)
```
