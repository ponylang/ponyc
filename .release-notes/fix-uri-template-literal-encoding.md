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
