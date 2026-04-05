## Add style/docstring-leading-blank lint rule

pony-lint now flags docstrings where a blank line immediately follows the opening `"""`. The first line of content should begin on the line right after the opening delimiter.

```pony
// Flagged — blank line after opening """
class Foo
  """

  Foo docstring.
  """

// Clean — content starts on the next line
class Foo
  """
  Foo docstring.
  """
```

Types and methods annotated with `\nodoc\` are exempt, consistent with `style/docstring-format`.
