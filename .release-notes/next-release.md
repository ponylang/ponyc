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

