"""
Test fixture for exercising LSP textDocument/selectionRange functionality.

Selection range returns a chain of progressively wider ranges enclosing the
cursor — used by editors to implement "expand selection" (e.g. Ctrl+Shift+→).
Each step in the chain must contain the previous step's range.

To manually test selection range functionality:
1. Open the lsp/test/workspace directory as a project in your editor.
2. Open selection_range/selection_range.pony while the Pony language
   server is active.
3. Place the cursor on any identifier or keyword and trigger "Expand
   Selection" (VS Code: Alt+Shift+→). Verify that each key-press selects
   a progressively larger syntactic unit: token → expression → function
   body → function → class body → class → file.
4. Place the cursor on a blank line and verify that Expand Selection does
   nothing (no range to expand to).

Expected expansion sequence for cursor on `x` in `fun value` (line 27):
  `x` → function body → `fun value` → `class SelectionRange` → file
"""

class SelectionRange
  """
  A simple class used to test selection range expansion.
  """
  fun value(x: U32): U32 =>
    x

  fun compute(a: U32, b: U32): U32 =>
    a + b
