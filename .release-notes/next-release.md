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

## Fix Format.int producing invalid UTF-8 with multi-byte fill characters

`Format.int` with a fill character outside the ASCII range produced invalid UTF-8. The formatter builds the digit string in reverse, then reverses it byte by byte — multi-byte fill characters had their UTF-8 bytes reversed along with the digits.

```pony
// U+00E9 is C3 A9 in UTF-8
Format.int[U8](0 where width = 2, fill = 0xE9)
// was: A9 C3 30 (invalid UTF-8)
// now: C3 A9 30 (valid UTF-8: "é0")
```

`Format` on strings with the same fill was unaffected.

## Fix Format.apply not applying prec for strings

`Format.apply` did not apply the `prec` parameter. `Format("hello" where prec = 2)` returned `"hello"` instead of `"he"`. When `width` was also set, output was wider than `width` — `Format("hello" where prec = 2, width = 4, align = AlignRight)` returned `"  hello"` (7 characters) instead of `"  he"` (4 characters).

## Fix NormalizeURI not lowercasing characters decoded from percent-encoding in the host

`NormalizeURI` left decoded uppercase letters in the host un-lowered. `NormalizeURI` on `http://%41B.example/` produced `http://Ab.example/` instead of `http://ab.example/`, and normalizing the result again changed it, breaking idempotency. `URIEquivalent` reported `http://%41B.example/` and `http://ab.example/` as not equivalent.

## Fix IRegexpCompiler.parse rejecting literal ^

`IRegexpCompiler.parse` rejected patterns containing a bare `^`, reporting a parse error at the caret's position. RFC 9485's NormalChar range includes `^` (0x5E) — I-Regexp has no anchors, so `^` is an ordinary literal outside `[...]`.

```pony
match IRegexpCompiler.parse("^")
| let re: IRegexp => re.is_match("^") // now: true
| let e: IRegexpParseError => e.string()
  // was: "I-Regexp error at offset 0: Invalid character encoding"
end
```

The escaped form `\^` (written `"\\^"` in a Pony string) was already accepted. Patterns like `"a^b"` now compile and match the literal string `"a^b"`.

## Fix IsPrime returning wrong results for primes near the type maximum

`IsPrime` reported primes near the maximum of an unsigned integer type as composite. For `U64`, primes near `U64.max_value()` caused the function to run indefinitely instead of returning.

```pony
IsPrime[U32](4294967291) // was false, now true (4294967291 is the largest U32 prime)
IsPrime[U16](65521)      // was false, now true (65521 is the largest U16 prime)
```

## Fix List.rvalues() and List.rnodes() yielding only the first element

Both methods returned only the first element of the list instead of iterating in reverse; they now yield every element from last to first.

```pony
let l = List[String].from(["a"; "b"; "c"])
for v in l.rvalues() do
  env.out.print(v)
end
// was:  a
// now:  c, b, a
```

## Fix signed mod producing wrong results when divisor is min_value

`mod` and `%%` on signed integers returned wrong results when the divisor was `min_value()` and the dividend was positive. The result depended on the optimization level — debug builds produced the correct value, release builds did not. `modc` had the same problem.

```pony
I32(1).mod(I32.min_value()) // was: wrong in release builds
                             // now: -2147483647
```

## Fix URITemplate.expand not percent-encoding non-ASCII literal characters

`URITemplate.expand` copied literal text from the template verbatim, so a template containing non-ASCII characters produced a result with raw UTF-8 bytes instead of a valid URI. Variable values in the same template were correctly percent-encoded.

```pony
// template with non-ASCII literal text
match URITemplateParse("café")
| let t: URITemplate =>
  t.expand(URITemplateVariables)
  // was: "café" (raw bytes C3 A9)
  // now: "caf%C3%A9"
end
```

## Fix HashMap.insert_if_absent returning wrong value when insertion triggers resize

`insert_if_absent` returned the wrong value when inserting a new key caused the map to grow. The value was stored correctly — only the return value was wrong.

## Fix _FormatInt allocation hint overflow when prec defaults to -1

`_FormatInt.u8`, `u16`, `u32`, and `u64` computed their initial `String` allocation as `(prec + 1).max(width.max(31))`. When `prec` was the default `-1` (`USize.max_value()`), `prec + 1` wrapped to 0, producing an allocation hint of `width.max(31)` regardless of the requested precision. The `u128` variant already used the resolved `prec'` and was unaffected.

All four methods now use `prec'` (the sentinel-resolved value) in their allocation hint, matching `u128`.

## Fix Iter.dedup() and Iter.unique() returning has_next() = true when exhausted

`Iter.dedup()` and `Iter.unique()` reported `has_next() = true` when the remaining source elements were all duplicates. `next()` would then error, violating the `Iterator` contract. This caused `last()`, `nth()`, and `find()` to error on valid input when trailing elements were duplicates.

```pony
// was: error
// now: 0
Iter[U64]([as U64: 0; 0].values()).dedup[HashEq[U64]]().last()?

// was: error
// now: 2
Iter[U64]([as U64: 1; 2; 1].values()).unique[HashEq[U64]]().last()?
```

## Fix help for nested CLI subcommands

The `help` subcommand and `--help` flag in the `cli` package could only show help for top-level commands. Requesting help for a nested command path — `app help config server` or `app config server --help` — failed with "too many positional arguments" or "unknown command" instead of printing the subcommand's help text.

Both paths now accept multi-level command paths and display the correct help.

## Fix Iter.take_while() returning rejected elements after has_next() returns false

`Iter.take_while()` had a bug where `has_next()` returned false on the first call after the predicate rejected an element, but true on the second call, and `next()` then yielded the rejected element. This broke `nth()` and any code that called `has_next()` more than once after the predicate failed.

```pony
// was: has_next() false, then true; next() returned 5
// now: has_next() stays false
let tw = Iter[U64]([as U64: 5; 1].values())
  .take_while({(x) => x < 3 })
tw.has_next() // false
tw.has_next() // false (was true)

// was: returned 5 (a rejected element)
// now: error
Iter[U64]([as U64: 5; 1].values())
  .take_while({(x) => x < 3 })
  .nth(1)?
```

## Fix Format.float ignoring align and fill parameters

`Format.float` accepted `align` and `fill` parameters but ignored them — output was always right-aligned with spaces regardless of the values passed. `Format.int` with the same arguments worked correctly.

```pony
Format.float[F64](1.5 where width = 8, align = AlignLeft, fill = '*',
  prec = 2, fmt = FormatFix)
// was: "    1.50"
// now: "1.50****"
```

## Fix float-to-integer saturation at the representability boundary

Safe float-to-integer conversions saturate out-of-range values to the target type's min or max. When the integer max was not exactly representable in the source float type, the float representation rounded up to the next power of two, and a float value at that exact boundary was not caught by the saturation check. The value fell through to the raw conversion, which is undefined behavior in LLVM and produced the target type's min on x86-64.

`F32(2147483648).i32()` returned `I32.min_value()` instead of `I32.max_value()`. The same problem affected `F64(9223372036854775808).i64()`, `F32(170141183460469231731687303715884105728).i128()`, `F32(4294967296).u32()`, and `F64(18446744073709551616).u64()`.

All float-to-integer conversions now saturate correctly at the boundary.

## Fix JSONPathParser rejecting whitespace between segments

`JSONPathParser` rejected valid JSONPath queries that had whitespace between segments, such as `$ .a`, `$.a .b`, and `$.a ['b']`. RFC 9535 section 2.1.1 allows optional whitespace before each segment. These queries now parse and evaluate correctly.

## Fix GreatestCommonDivisor returning negative values for signed inputs

`GreatestCommonDivisor` returned a negative value when the second argument was negative. The greatest common divisor is positive by definition.

```pony
GreatestCommonDivisor[I64](4, -6)?  // was: -2, now: 2
GreatestCommonDivisor[I64](-4, -6)? // was: -2, now: 2
GreatestCommonDivisor[I8](1, -1)?   // was: -1, now: 1
```

When both arguments are `min_value()` of a signed type, the positive result cannot be represented and the call now errors. Previously it silently returned the negative `min_value()`.

## Fix JSONPathParser accepting integers RFC 9535 does not allow

`JSONPathParser` accepted queries with negative zero (`$[-0]`), leading zeros (`$[01]`), and a lone minus sign (`$[-:]`) in index and slice positions. RFC 9535 section 2.3.3.1 defines `int = "0" / (["-"] DIGIT1 *DIGIT)`, so these are not well-formed and must be rejected. These queries now return `JSONPathParseError`.

## Fix URITemplate prefix modifier splitting pct-encoded triplets under reserved expansion

`URITemplate.expand` with a prefix modifier (`:N`) under reserved (`+`) or fragment (`#`) expansion split pct-encoded triplets in the variable value. RFC 6570 counts a `%XX` triplet as one character for prefix length; the expansion counted each byte separately, cut after the `%`, and re-encoded the lone `%`.

```pony
let vars = URITemplateVariables
vars.set("y", "%41b")
match URITemplateParse("{+y:1}")
| let t: URITemplate =>
  t.expand(vars)
  // was: "%25"
  // now: "%41"
end
```

## Fix Base64.encode_pem and Base64.encode_mime producing trailing empty lines

`Base64.encode_pem` and `Base64.encode_mime` appended an extra CRLF when the input was empty or when its encoding filled the last line exactly (multiples of 48 bytes for PEM, 57 for MIME). Empty input produced `"\r\n"` instead of `""`, and exact-line inputs ended with `"\r\n\r\n"` instead of `"\r\n"`.

## Fix Iter.maybe, dedup, and take_while dropping None elements

`Iter.maybe`, `Iter.dedup`, and `Iter.take_while` silently dropped `None` elements when the element type was `None` or a union containing `None`, such as `(String | None)`.

All three methods now handle `None` elements correctly. One behavior change: `Iter[(T | None)].maybe(None)` now produces a one-element iterator containing `None` rather than an empty iterator.

## Fix String.utf32 decoding UTF-8-encoded surrogates instead of rejecting them

`String.utf32` decoded the byte sequences for surrogate code points U+D800 through U+DFFF as valid characters. RFC 3629 forbids these sequences in UTF-8, and the `utf32` docstring promises `(0xFFFD, 1)` for invalid encodings. `runes` inherited the problem through `utf32`.

```pony
let s = recover val String.from_array([0xED; 0xA0; 0x80]) end
s.utf32(0)? // was: (0xD800, 3), now: (0xFFFD, 1)
```

## Fix CommandParser rejecting negative number positional arguments

`CommandParser` rejected negative numbers as positional arguments, reporting them as unknown short options. `app -5` with an `ArgSpec.i64` positional returned `SyntaxError: unknown short option at: '5'` instead of parsing `-5` as the argument value. The same problem affected `ArgSpec.f64` with values like `-1.5`.

Negative numbers as positional arguments now parse directly. The `--` separator and named-option forms (`-n -5`, `--num=-5`) continue to work.

```pony
let spec = CommandSpec.leaf("app", "", Array[OptionSpec](), [
  ArgSpec.i64("n", "")
])?
let cmd = CommandParser(spec).parse(["app"; "-5"]) as Command
cmd.arg("n").i64() // was: SyntaxError, now: -5
```

## Fix JSONPathParser accepting invalid string literals

`JSONPathParser.parse` accepted JSONPath queries containing raw control characters (U+0000 through U+001F) in string literals, and accepted `\'` escapes in double-quoted strings and `\"` escapes in single-quoted strings. RFC 9535 section 2.3.1.1 requires raw control characters to be escaped and restricts each quote escape to its own quote type. These queries now return `JSONPathParseError`.

```pony
// These were accepted, now rejected:
JSONPathParser.parse("$['a\x01b']")  // raw control character
JSONPathParser.parse("$[\"\\'\"]")   // \' in a double-quoted string
JSONPathParser.parse("$['\\\"']")    // \" in a single-quoted string

// Escaped control characters remain valid:
JSONPathParser.parse("$['a\\u0001b']") // \u0001 is the escaped form
```

## Fix Iter.cycle() reporting has_next() = true on an empty source iterator

`Iter.cycle()` on an empty iterator reported `has_next() = true` indefinitely while every call to `next()` raised an error. Code following the `has_next()` protocol — including `for` loops and other `Iter` adapters like `chain` — either looped forever or, in the case of `chain`, silently dropped every iterator after the empty cycle in the pipeline.

`cycle()` now returns `false` from `has_next()` when the source is empty.

## Fix ParseURI accepting hosts containing ':'

`ParseURI` and `ParseURIAuthority` accepted non-bracketed hosts containing `:`, which RFC 3986 forbids in reg-name.

The parsed URI's `string()` dropped the empty port, producing output that `ParseURI` then rejected — `parse(uri.string())` failed for a URI that `ParseURI` itself returned.

```pony
ParseURI("http://a:b:/")
// was: parsed with host "a:b"; string() "http://a:b/" rejected on reparse
// now: rejected with InvalidHost
```

