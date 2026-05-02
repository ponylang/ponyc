"""
Test fixtures for exercising LSP type hierarchy functionality.

This package provides fixture files for manual and automated testing of the
three LSP type hierarchy operations (prepareTypeHierarchy,
typeHierarchy/supertypes, and typeHierarchy/subtypes).

To manually test type hierarchy functionality:
1. Open the lsp/test/workspace directory as a project in your editor while
   the Pony language server is active.
2. Open _t_hier_b.pony and place your cursor on `_THierA`, `_THierB`, or
   `_THierC`, then invoke "Show Type Hierarchy" from the editor menu or
   command palette.
3. Open _t_hier_isect.pony and place your cursor on `_THierIsect` to verify
   that intersection-type provides clauses expand into both supertypes.
4. Open _t_hier_d.pony and place your cursor on `_THierD` to verify that
   subtypes defined in other files are found.

Expected type hierarchy behavior:
- prepareTypeHierarchy returns a single item when the cursor is on a type
  name, or null when the cursor is not on a type name
- typeHierarchy/supertypes returns the types in the entity's provides clause,
  including both sides of intersection types
- typeHierarchy/subtypes returns all types in the compiled package that
  provide the queried type, even when defined in other files
- Both supertypes and subtypes return an empty array (not null) when there
  are no results
"""
