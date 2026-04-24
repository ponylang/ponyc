## Fix LSP outline including synthetic and inherited members

Previously, the `textDocument/documentSymbol` response (used by editors to build the outline/symbol tree) included members that were not explicitly written in a class's source file. A bare class that inherited a trait with a default method, or any class without an explicit constructor, would have those synthesized or inherited members appear as children in the outline.

This has been fixed. The outline now shows only members that are explicitly written in the file being viewed.
