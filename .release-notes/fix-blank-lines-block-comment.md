## Fix `style/blank-lines` false positive on block comments between declarations

`pony-lint`'s `style/blank-lines` rule fired on block comments placed between type declarations when the comment contained blank lines (paragraph breaks). No arrangement of blank lines around the comment satisfied the rule:

```pony
class val Foo
  let x: U8
  new val create(x': U8) => x = x'

/*
Comment with

a paragraph break.
*/
type Bar is (Foo | None)
```

Blank lines inside `/* */` comments are now excluded from the between-entities count. A blank line before the comment still satisfies the one-blank-line requirement between declarations.
