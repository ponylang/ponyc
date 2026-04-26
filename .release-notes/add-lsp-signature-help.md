## Add LSP `textDocument/signatureHelp` support

The Pony language server now supports `textDocument/signatureHelp`, providing parameter hints when the cursor is inside a call expression. The popup shows the full method signature with the active parameter highlighted, and includes the method's docstring when present.

Triggered by `(` and `,` characters, consistent with standard LSP conventions.

Signature help is driven by the compiled AST and requires the file to be saved — it is not available mid-keystroke while the file has unsaved edits.
