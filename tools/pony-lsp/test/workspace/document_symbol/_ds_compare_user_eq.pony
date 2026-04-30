// Fixture for `document_symbol/integration/primitive_partial_synthesis`.
//
// Open this file in an editor with pony-lsp running and check the outline
// (document symbol) panel. `_DsCompareUserEq` should appear with exactly
// one child: `eq`. The synthesized `ne` must not appear.
// A regression shows `ne` nested under it.
//
// IMPORTANT: do not add entities below this primitive. The
// primitive_partial_synthesis test depends on it being the last entity in
// the file (max_pos is None).

primitive _DsCompareUserEq
  fun eq(that: _DsCompareUserEq box): Bool => this is that
