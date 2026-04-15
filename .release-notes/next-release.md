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

## Add LSP `textDocument/rename` and `textDocument/prepareRename` support

The Pony language server now supports symbol rename. Placing the cursor on any renameable identifier — field, method, behaviour, local variable, parameter, type parameter, class, actor, struct, primitive, trait, or interface — and invoking Rename Symbol in your editor will produce a `WorkspaceEdit` replacing every occurrence across all packages in the workspace.

`textDocument/prepareRename` is also implemented, allowing editors to validate that the cursor is on a renameable symbol before prompting for the new name. The server advertises `prepareProvider: true` in its capabilities.

Renames are rejected with an appropriate error when:

- The cursor is on a literal or synthetic expression node.
- The target symbol is defined outside the workspace (stdlib or external package).
- The supplied new name is not a valid Pony identifier.

## Add LSP `textDocument/typeDefinition` support

The Pony language server now supports Go to Type Definition. Placing the cursor on any symbol with a known type — a local variable, parameter, or field — and invoking Go to Type Definition in your editor will navigate to the declaration of the symbol's type rather than the symbol itself.

This works for explicitly annotated bindings (`let x: MyClass`) and for bindings whose type is inferred (`let x = MyClass.create()`).

