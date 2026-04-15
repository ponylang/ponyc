## Add LSP `textDocument/rename` and `textDocument/prepareRename` support

The Pony language server now supports symbol rename. Placing the cursor on any renameable identifier — field, method, behaviour, local variable, parameter, type parameter, class, actor, struct, primitive, trait, or interface — and invoking Rename Symbol in your editor will produce a `WorkspaceEdit` replacing every occurrence across all packages in the workspace.

`textDocument/prepareRename` is also implemented, allowing editors to validate that the cursor is on a renameable symbol before prompting for the new name. The server advertises `prepareProvider: true` in its capabilities.

Renames are rejected with an appropriate error when:

- The cursor is on a literal or synthetic expression node.
- The target symbol is defined outside the workspace (stdlib or external package).
- The supplied new name is not a valid Pony identifier.
