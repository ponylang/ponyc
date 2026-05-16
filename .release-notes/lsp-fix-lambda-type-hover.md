## Fix LSP hover for lambda types

Hovering over a field, parameter, or variable with a lambda type annotation now shows the human-readable lambda type instead of a compiler-internal hygienic ID.

Before:

```pony
let _callback: $0 val
```

After:

```pony
let _callback: {(String val): None val} val
```
