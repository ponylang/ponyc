"""
Test fixtures for exercising LSP goto definition functionality.

This module provides test cases for manual and automated testing of goto
definition functionality provided by the Pony language server (LSP).

To manually test goto definition functionality:
1. Open the lsp/test/workspace directory as a project in your editor.
2. Open _class.pony while the Pony language server is active.
3. Place your cursor on any of the following and invoke goto definition:
   - `_value` in the constructor body or `get` method body — navigates to
     the field declaration
   - `v` in the constructor body — navigates to the parameter declaration
   - `get` in the `demo` method body — navigates to the method declaration
   - `this` in the `demo` method body — navigates to the class declaration

Expected goto definition behavior:
- Navigate to the declaration of the symbol under the cursor
- Return no result when the cursor is on a docstring, a keyword (`class`,
  `fun`, `new`), an operator, or any other non-symbol token
"""
