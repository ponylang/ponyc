## Add GNU Info backend to pony-doc

pony-doc can now generate GNU Info documentation with `--backend info`. The output is a single `.info` file readable by any Info viewer — Emacs, standalone `info`, or `pinfo`.

```bash
pony-doc --backend info ./my-package
```

The default backend remains `mkdocs`, so existing invocations are unchanged.

## Fix Base64.encode_url emitting NUL bytes instead of stripping padding

`Base64.encode_url` with the default `pad = false` wrote NUL bytes where padding characters would go, instead of omitting them. The NUL-padded output was then rejected by `Base64.decode_url` as invalid base64.

`Base64.encode_url` now strips padding entirely when `pad = false`.

## Fix String.find and contains matching past the end of the string

`String.find`, `contains`, and `rfind` compared the needle byte by byte without checking that it fit within the string at each candidate position. When the needle extended past the string's size, the comparison read the null terminator or, for a trim view sharing a larger buffer, bytes from the parent string. `count` and `split_by` inherited the problem through `find`.

```pony
let s: String val = "abcd".trim(0, 2) // "ab", backed by "abcd"
s.contains("bc") // was true, now false
s.find("bc")?     // was 1, now raises error
s.count("bc")     // was 1, now 0
```

## Fix Format.float with FormatFix truncating output longer than 30 characters

`Format.float` used a fixed 31-byte buffer for the formatted output. Values that need more than 30 characters in `FormatFix` notation — any float whose integer part exceeds 30 digits, such as `1e300` — were silently truncated.

```pony
// was: "100000000000000005250476025520" (30 chars, truncated)
// now: the full 301-digit representation
Format.float[F64](1e300 where fmt = FormatFix, prec = 0)
```

## Fix Sort stack overflow on degenerate inputs

Sorting an already-sorted, reverse-sorted, or all-equal array of moderate size (around 100,000 elements or more, depending on the platform's stack limit) caused a stack overflow. Sorting these inputs now completes in O(n log n) time regardless of input pattern.

## Fix Base64.decode returning extra bytes for unpadded input with non-zero trailing bits

When unpadded Base64 input does not divide evenly into bytes, the leftover bits at the end are padding and must be discarded. `Base64.decode` and `decode_url` instead emitted those trailing bits as an extra byte when they were non-zero. A single Base64 character (6 bits) incorrectly decoded to one byte, and two characters (12 bits) produced two bytes instead of one.

```pony
// Before:
Base64.decode[Array[U8] iso]("Q")?   // [64] — wrong, 6 bits can't fill a byte
Base64.decode[Array[U8] iso]("QR")?  // [65; 16] — wrong, trailing 4 bits emitted

// After:
Base64.decode[Array[U8] iso]("Q")?   // [] — 6 bits, no complete byte
Base64.decode[Array[U8] iso]("QR")?  // [65] — 12 bits, one byte plus discarded padding
```

## Fix JSONParser.parse accepting trailing incomplete tokens

`JSONParser.parse` accepted input containing a valid JSON value followed by an incomplete token — for example, `"nulln"` or `1 "x`. Instead of reporting an error, it silently ignored the trailing content and returned the first value. It now returns a parse error for any input where content remains after the first value.

## Fix Format.int with FormatUTF32 printing decimal digits for U8, U16, I8, and I16

`Format.int` with `FormatUTF32` produced the decimal representation instead of the UTF-32 character for `U8`, `U16`, `I8`, and `I16` values.

```pony
// was: "65"
// now: "A"
Format.int[U8](65, FormatUTF32)
```

