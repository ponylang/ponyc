// Fixture for `document_symbol/integration/primitive_no_children`.
//
// Open this file in an editor with pony-lsp running and check the outline
// (document symbol) panel. `_DsComparable` should appear with no children.
// A regression shows `eq` and `ne` nested under it.

// IMPORTANT: do not add entities below this primitive. The
// primitive_no_children test depends on it being the last entity in the
// file (max_pos is None); adding an entity below would provide a max_pos
// that culls synthesized eq/ne even without the BUILD-position filter,
// causing the test to pass with the filter removed.
primitive _DsComparable
