## Fix Format.int with FormatUTF32 ignoring width, fill, and align

`Format.int` with `FormatUTF32` returned the bare character and ignored `width`, `fill`, and `align`. Padding and alignment are now applied.
