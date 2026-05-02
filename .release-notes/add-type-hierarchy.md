## Add LSP type hierarchy support

The Pony language server now supports the LSP type hierarchy protocol:

- `textDocument/prepareTypeHierarchy` — places a cursor on a class, trait,
  actor, interface, primitive, or struct and returns a `TypeHierarchyItem`
  describing it.
- `typeHierarchy/supertypes` — returns items for each type in the entity's
  `is` (provides) list.
- `typeHierarchy/subtypes` — cross-package walk that returns items for every
  entity whose `is` list directly includes the given type.

Editors that support this protocol (VS Code, Neovim, etc.) expose it as
"Show Type Hierarchy", "Go to Supertypes", and "Go to Subtypes" commands.
