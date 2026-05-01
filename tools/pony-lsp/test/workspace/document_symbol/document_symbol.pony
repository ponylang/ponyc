"""
Test fixtures for exercising LSP textDocument/documentSymbol functionality.

Each file in this package targets a specific slice of `DocumentSymbols`:

  _ds_containment.pony — layout mirrors the references fixture; drives
    the containment and range integration tests (selectionRange ⊆ range,
    full-declaration range spans past the keyword).

  _ds_ent_interface.pony — one of every top-level entity token
    (`tk_class`, `tk_actor`, `tk_trait`, `tk_interface`, `tk_primitive`,
    `tk_struct`, `tk_type`) to guard the entity → SymbolKind map in
    `DocumentSymbols.from_module`.

  member_kinds.pony — one of every member token (`tk_new`, `tk_fun`,
    `tk_be`, `tk_flet`, `tk_fvar`, `tk_embed`) under an actor so
    behaviours are legal; guards the member → SymbolKind map in
    `DocumentSymbols.find_members`.

  _ds_trait.pony / _ds_impl.pony — trait with a default method body and
    a non-overriding implementer. Regression guard for the source-file
    filter in `ASTSourceSpan`: if the filter drops, the impl class's
    range.end.line inflates into the trait file's lines.

  _ds_comparable.pony — bare primitive as the last entity in its file.
    Regression guard for synthesized `eq`/`ne` (from `add_comparable`)
    leaking into the outline when `max_pos` is `None`.

  _ds_compare_user_eq.pony — primitive with user-written `eq` as the last
    entity in its file. Regression guard for the partial-synthesis case:
    ponyc skips synthesizing `eq` (already present) but still synthesizes
    `ne`; the BUILD-position filter must suppress `ne` while preserving `eq`.
"""
