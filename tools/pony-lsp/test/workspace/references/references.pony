"""
Test fixtures for exercising LSP find-references functionality.

Open referenced_class.pony or references_user.pony in an LSP-aware editor
while the Pony language server is active. Place the cursor on any symbol
described in the class docstrings below and invoke Find All References.

The editor should show all locations where that symbol is used across the
workspace, including both files. Placing the cursor on a declaration or any
reference to the same symbol produces the same set of locations.

To test `includeDeclaration` behaviour: some editors expose this as an option.
When disabled, the declaration site should not appear in the results list.
"""
