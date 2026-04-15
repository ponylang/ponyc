"""
Test fixtures for exercising LSP rename functionality.

Open renameable.pony or rename_user.pony in an LSP-aware editor while the
Pony language server is active. Place the cursor on any symbol described in
the class docstrings and invoke Rename Symbol (typically F2).

The editor should prompt for a new name, then apply edits across every file
in the workspace that references that symbol. The WorkspaceEdit covers both
the declaration site and all use sites.

Symbols defined in the stdlib or outside the workspace are rejected: the
server returns an error and no edits are applied.
"""
