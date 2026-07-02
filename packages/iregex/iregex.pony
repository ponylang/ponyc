"""
# iregex

I-Regexp (RFC 9485) pattern matching for Pony. I-Regexp is a
deliberately constrained regular expression syntax designed for
interoperability across implementations. It supports:

* Literal characters and Unicode codepoints
* Character classes (`[a-z]`, `[^0-9]`), including Unicode properties
  (`\\p{L}`, `\\P{Nd}`)
* Quantifiers (`*`, `+`, `?`, `{n}`, `{n,m}`, `{n,}`)
* Alternation (`a|b`) and grouping (`(ab)+`)
* Dot (`.`) matching any codepoint except `\\n` and `\\r`
* Control character escapes (`\\n`, `\\r`, `\\t`)

## Usage

Parse a pattern string into a compiled regexp, then match or search:

```pony
use iregex = "iregex"

// Parse returns errors as data — no exceptions to catch
match iregex.IRegexpCompiler.parse("[a-z]+")
| let re: iregex.IRegexp =>
  re.is_match("hello")   // true — full-string match
  re.search("abc123def") // true — substring match
| let err: iregex.IRegexpParseError =>
  env.out.print("Error: " + err.string())
end

// compile() raises on invalid input — use for known-valid patterns
try
  let re = iregex.IRegexpCompiler.compile("[0-9]{3}")?
  re.is_match("456") // true
end
```

`is_match()` tests if the entire input matches. `search()` tests if
any substring matches.
"""
