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
