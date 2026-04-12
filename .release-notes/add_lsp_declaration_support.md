## Add LSP `textDocument/declaration` support

The Pony language server now handles `textDocument/declaration` requests. In Pony there are no separate declaration sites — declaration and definition are always the same location — so the handler routes directly to the existing go-to-definition implementation. The server advertises `declarationProvider: true` in its capabilities.
