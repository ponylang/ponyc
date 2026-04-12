## Extend LSP inlay hints with generic caps, return type caps, and receiver caps

The pony-lsp inlay hint feature now covers additional implicit capability annotations:

- **Generic type annotations**: capability hints on type arguments in generic types, including nested generics, union/intersection/tuple members, class fields (`let`, `var`, `embed`), and function return types.
- **Receiver capability**: a hint after `fun` showing the implicit capability (e.g. `box`) when no explicit cap keyword is written. Not emitted for `be` or `new`.
- **Return type capability**: when a function has an explicit return type annotation, a capability hint is added after the type name if the cap is absent.
- **Inferred return type**: when a function has no return type annotation, a hint shows the full inferred return type (e.g. `: None val`).
