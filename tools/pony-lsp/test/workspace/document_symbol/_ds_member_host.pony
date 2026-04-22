// Fixture for `document_symbol/integration/member_kinds` — exercises every
// member token that `DocumentSymbols.find_members` maps to an LSP SymbolKind
// (`tk_new` → constructor; `tk_fun`/`tk_be` → method; `tk_flet`/`tk_fvar`/
// `tk_embed` → field).
//
// Regression guard against silent edits to the member `tk_*` → SymbolKind
// match table at `tools/pony-lsp/symbols.pony:214-220`. Actor so that
// behaviours (`be`) are legal.

actor _DsMemberHost
  let _let_field: U32 = 0
  var _var_field: U32 = 0
  embed _embed_field: _DsEmbedTarget = _DsEmbedTarget

  new create() =>
    None

  fun ref ds_fun(): U32 =>
    _var_field

  be ds_be() =>
    None

class _DsEmbedTarget
