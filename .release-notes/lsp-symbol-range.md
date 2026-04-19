## Fix LSP symbol range semantics

The `textDocument/documentSymbol` response was returning the full declaration range for both the `range` and `selectionRange` fields, losing the identifier-only span that `selectionRange` is meant to carry. Editors use `selectionRange` to position the cursor on the symbol name rather than the start of its declaration.

The `workspace/symbol` response was returning the identifier-only span for `SymbolInformation.location.range`, where the LSP spec expects the full declaration range.

Both endpoints are now spec-compliant and consistent with each other.
