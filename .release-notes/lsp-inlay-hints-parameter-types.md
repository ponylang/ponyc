## Add pony-lsp inlay hints for function parameter types

pony-lsp now shows capability hints on function parameter type annotations where the capability is omitted from source.

```diff
-fun box greet(name: String, items: Array[String]): None val =>
+fun box greet(name: String val, items: Array[String val] ref): None val =>
   None
```

Hints appear after each type name (and after `]` for generic types). Parameters with explicit capabilities are unaffected — no hint is shown when the capability is already written out.

Type parameter references (e.g. `T` in `Array[T]`) have no fixed capability, so they produce no hint.
