## Fix Base64.decode accepting arbitrary bytes after padding

`Base64.decode` stopped reading at the first `=` and never checked what followed. Trailing garbage, concatenated messages, or arbitrary bytes after padding were silently accepted.

```pony
Base64.decode[String iso]("QQ==garbage!!")?  // was: ok "A", now: error
Base64.decode[String iso]("QQ==QkM=")?       // was: ok "A", now: error
Base64.decode[String iso]("QQ==!")?          // was: ok "A", now: error
```

The decoder now validates the remainder after the first `=` and errors on any byte that isn't whitespace or another `=`.
