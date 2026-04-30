## Fix match exhaustiveness for Bool value patterns in tuples

Previously, matching on a `Bool` inside a tuple required an `else` clause even when both `true` and `false` were covered:

```pony
primitive Foo
  fun apply(x: (String, Bool)): Bool =>
    match x
    | (_, true) => true
    | (_, false) => false
    end
```

```
Error:
main.pony:3:5: function body isn't the result type
```

This has been fixed. Bool value patterns inside tuples now participate in exhaustiveness checking, so `(_, true)` and `(_, false)` correctly cover `(String, Bool)`. This also works with nested tuples, multiple Bool elements, and Bool type aliases.
