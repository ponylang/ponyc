## Fix LSP symbol ranges

LSP clients that use `textDocument/documentSymbol` (the outline/breadcrumb view in most editors) could produce a "selectionRange must be contained in range" error, causing the entire symbol list to be rejected. This is now fixed.

In addition, symbol ranges across both `textDocument/documentSymbol` and `workspace/symbol` now correctly cover the full declaration — from the opening keyword to the end of the body. Previously, `textDocument/documentSymbol` ranges covered only the declaration keyword (`class`, `fun`, etc.), and `workspace/symbol` ranges covered only the identifier. Highlighting a symbol or jumping to it now selects the whole declaration.
