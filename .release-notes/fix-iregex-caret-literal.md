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
