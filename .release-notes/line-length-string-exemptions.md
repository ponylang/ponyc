## Exempt unsplittable string literals from line length rule

The `style/line-length` lint rule no longer flags lines where the only reason for exceeding 80 columns is a string literal that contains no spaces. Strings without spaces — URLs, file paths, qualified identifiers — cannot be meaningfully split across lines, so flagging them produced noise with no actionable fix.

Strings that contain spaces are still flagged because they can be split at space boundaries using compile-time string concatenation at zero runtime cost:

```pony
// Before: flagged, and splitting is awkward
let url = "https://github.com/ponylang/ponyc/blob/main/packages/builtin/string.pony"

// After: no longer flagged — the string has no spaces and can't be split

// Strings with spaces can still be split, so they remain flagged:
let msg = "This is a very long error message that should be split across multiple lines"

// Fix by splitting at spaces:
let msg =
  "This is a very long error message that should be split "
  + "across multiple lines"
```

Lines inside triple-quoted strings (docstrings) and lines containing `"""` delimiters are not eligible for this exemption — docstring content should be wrapped regardless of whether it contains spaces.
