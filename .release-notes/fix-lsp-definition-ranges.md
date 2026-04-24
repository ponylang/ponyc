## Fix LSP definition and type-definition ranges

`textDocument/definition` and `textDocument/typeDefinition` responses now return a range that covers the full declaration — from the opening keyword to the end of the body. Previously the range covered only the declaration keyword (`class`, `fun`, etc.).

Range computation now also correctly handles the last type declaration in a file. Previously the compiler's synthesized default constructors could cause the final entity's range to extend to an incorrect position; this no longer occurs.
