## Fix pony-lsp document symbols showing spurious eq/ne for bare primitives

The document symbol outline (textDocument/documentSymbol) incorrectly included `eq` and `ne` entries for bare primitives that appeared last in their file. These methods are synthesized by the compiler and should not appear in the outline. They now correctly have no children.
