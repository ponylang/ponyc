## Fix triple-quoted string preserving \r from CRLF line endings

Triple-quoted strings in source files with CRLF line endings produced `\r\n` in the string content instead of `\n`. The `\r` remained in the output on every interior line.

```pony
// source file saved with CRLF line endings:
let s = """
    Foo
    bar
  """
// was: "Foo\r\nbar\r\n"
// now: "Foo\nbar\n"
```
