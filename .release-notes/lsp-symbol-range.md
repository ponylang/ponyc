## Fix LSP symbol range semantics

The `textDocument/documentSymbol` response was returning the keyword-only span for both the `range` and `selectionRange` fields, losing the identifier-only span that `selectionRange` is meant to carry and truncating `range` so that it no longer enclosed the declaration. Editors use `selectionRange` to position the cursor on the symbol name rather than the start of its declaration, and the LSP spec requires `selectionRange` to be contained within `range` — some clients reject the response outright with "selectionRange must be contained in range" when it is not.

The `workspace/symbol` response was returning the identifier-only span for `SymbolInformation.location.range`, where the LSP spec expects the full declaration range.

Both endpoints now return the full declaration range (the entity keyword through the end of its body) as `range`, and the identifier-only span as `selectionRange`, and are consistent with each other.
