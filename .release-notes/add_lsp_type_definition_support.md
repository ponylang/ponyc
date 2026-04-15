## Add LSP `textDocument/typeDefinition` support

The Pony language server now supports Go to Type Definition. Placing the cursor on any symbol with a known type — a local variable, parameter, or field — and invoking Go to Type Definition in your editor will navigate to the declaration of the symbol's type rather than the symbol itself.

This works for explicitly annotated bindings (`let x: MyClass`) and for bindings whose type is inferred (`let x = MyClass.create()`).
