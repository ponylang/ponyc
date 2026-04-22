## Add LSP `textDocument/selectionRange` support

The Pony language server now handles `textDocument/selectionRange` requests, enabling editors to expand the selection to progressively larger syntactic units (e.g. Alt+Shift+→ in VS Code).

For a given cursor position the server returns a chain of nested ranges — innermost first — walking up the AST from the token under the cursor through its enclosing expressions, function, class body, and finally the whole file. Ancestor nodes whose span is identical to their child are collapsed so that each step in the chain produces a visible selection change. Descendant nodes from other source files (such as trait methods merged into a class by the compiler) are excluded so that the ranges always stay within the current file.
