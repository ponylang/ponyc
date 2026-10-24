## Fix triple-quoted string dropping whitespace-only lines shorter than the indentation

A whitespace-only line inside a triple-quoted string vanished when it had fewer spaces than the common indentation. An empty line (zero spaces) or a line with at least as many spaces as the indentation both produced an empty line in the result, but a line with fewer spaces than the indentation was removed entirely, newline included.

```pony
let s = """
      first
  
      second
    """
// was: "first\nsecond\n" (the middle line and its newline are gone)
// now: "first\n\nsecond\n"
```
