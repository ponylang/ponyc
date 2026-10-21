## Fix URITemplateParse accepting prefix lengths with leading zeros

`URITemplateParse` accepted templates like `{a:01}` and `{a:0009}` as valid prefix modifiers. RFC 6570 section 2.4.1 defines `max-length = %x31-39 0*3DIGIT` — the first digit must be 1–9, so a leading zero is invalid syntax. These templates now produce a `URITemplateParseError`.

```pony
// Before: accepted as prefix length 1
URITemplateParse("{a:01}")  // -> URITemplate

// After: returns an error
URITemplateParse("{a:01}")  // -> URITemplateParseError
```
