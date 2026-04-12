## Add LSP `workspace/inlayHint/refresh` support

After each compilation, pony-lsp now sends a `workspace/inlayHint/refresh` request to the editor, asking it to re-request inlay hints for all open documents. Previously, inlay hints (such as inferred type annotations) would not update after a file was saved and recompiled. This only takes effect when the editor advertises support for `workspace/inlayHint/refresh` in its LSP capabilities (all major editors do).

## Extend LSP inlay hints with generic caps, return type caps, and receiver caps

The pony-lsp inlay hint feature now covers additional implicit capability annotations:

- **Generic type annotations**: capability hints on type arguments in generic types, including nested generics, union/intersection/tuple members, class fields (`let`, `var`, `embed`), and function return types.
- **Receiver capability**: a hint after `fun` showing the implicit capability (e.g. `box`) when no explicit cap keyword is written. Not emitted for `be` or `new`.
- **Return type capability**: when a function has an explicit return type annotation, a capability hint is added after the type name if the cap is absent.
- **Inferred return type**: when a function has no return type annotation, a hint shows the full inferred return type (e.g. `: None val`).

## Add LSP `textDocument/declaration` support

The Pony language server now handles `textDocument/declaration` requests. In Pony there are no separate declaration sites — declaration and definition are always the same location — so the handler routes directly to the existing go-to-definition implementation. The server advertises `declarationProvider: true` in its capabilities.

