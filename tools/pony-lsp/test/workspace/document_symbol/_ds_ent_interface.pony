// Fixture for `document_symbol/integration/entity_kinds` — exercises every
// top-level entity token that `DocumentSymbols.from_module` maps to an LSP
// SymbolKind (class, actor, trait, interface, primitive, struct, type).
//
// Regression guard against silent edits to the `tk_*` → SymbolKind match
// table at `tools/pony-lsp/symbols.pony:153-163`.

class _DsEntClass

actor _DsEntActor

trait _DsEntTrait
  fun ds_trait_method(): U32

interface _DsEntInterface
  fun ds_interface_method(): U32

primitive _DsEntPrimitive

struct _DsEntStruct
  let _f: U32 = 0

type _DsEntType is _DsEntClass
