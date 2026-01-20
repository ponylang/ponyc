use "assert"

use "ast"
use "immutable-json"

primitive SymbolKinds
  fun tag file(): I64 => 1
  fun tag module(): I64 => 2
  fun tag namespace(): I64 => 3
  fun tag package(): I64 => 4
  fun tag sk_class(): I64 => 5
  fun tag method(): I64 => 6
  fun tag property(): I64 => 7
  fun tag field(): I64 => 8
  fun tag constructor(): I64 => 9
  fun tag enum(): I64 => 10
  fun tag sk_interface(): I64 => 11
  fun tag function(): I64 =>12
  fun tag variable(): I64 =>13
  fun tag constant(): I64 =>14
  fun tag string(): I64 =>15
  fun tag number(): I64 =>16
  fun tag boolean(): I64 =>17
  fun tag array(): I64 =>8
  fun tag sk_object(): I64 =>19
  fun tag key(): I64 =>20
  fun tag null(): I64 =>21
  fun tag enum_member(): I64 =>22
  fun tag sk_struct(): I64 =>23
  fun tag event(): I64 =>24
  fun tag operator(): I64 =>25
  fun tag type_parameter(): I64 =>26

primitive SymbolTags
  fun tag deprecated(): I64 => 1

class DocumentSymbol
  let name: String
  let detail: (String | None)
    """More detail for this symbol, e.g the signature of a function."""
  let kind: I64
    """
    The kind of this symbol. See [SymbolKinds](lsp-SymbolKinds.md).
    """
  let tags: (Array[I64] ref | None)
    """
    Tags for this document symbol.

	since 3.16.0
    """
  let range: LspPositionRange
    """
    The range enclosing this symbol not including leading/trailing whitespace
	  but everything else like comments. This information is typically used to
	  determine if the clients cursor is inside the symbol to reveal in the
	  symbol in the UI.
    """
  let selection_range: LspPositionRange
    """
    The range that should be selected and revealed when this symbol is being
	  picked, e.g. the name of a function. Must be contained by the `range`.
    """

  let children: Array[DocumentSymbol] ref
    """
    Children of this symbol, e.g. properties of a class.
    """

  new ref create(
    name': String,
    kind': I64,
    range': LspPositionRange,
    selection_range': LspPositionRange)
  =>
    name = name'
    detail = None
    kind = kind'
    tags = None
    range = range'
    selection_range = selection_range'
    children = Array[DocumentSymbol].create()

  fun ref push_child(child: DocumentSymbol) =>
    this.children.push(child)

  fun to_json(): JsonType =>
    var builder = Obj(
        "name", this.name)(
        "kind", this.kind)(
        "range", this.range.to_json())(
        "selectionRange", this.range.to_json())
    if this.detail isnt None then
        builder = builder("detail", detail)
    end
    try
        var json_tags = Arr
        for tagg in (this.tags as this->Array[I64]).values() do
            json_tags = json_tags(tagg)
        end
        builder = builder("tags", json_tags)
    end
    if this.children.size() > 0 then
      var json_children = Arr
      for child in this.children.values() do
        json_children = json_children(child.to_json())
      end
      builder = builder("children", json_children)
    end

    builder.build()

primitive DocumentSymbols
  fun tag from_module(module: Module, channel: Channel): Array[DocumentSymbol] ref =>
    let symbols: Array[DocumentSymbol] ref = Array[DocumentSymbol].create(4)
    for module_child in module.ast.children() do
      let maybe_kind = match module_child.id()
      | TokenIds.tk_interface()
      | TokenIds.tk_trait() => SymbolKinds.sk_interface()
      | TokenIds.tk_primitive()
      | TokenIds.tk_class()
      | TokenIds.tk_type()
      | TokenIds.tk_actor() => SymbolKinds.sk_class()
      | TokenIds.tk_struct() => SymbolKinds.sk_struct() // TODO: really use a struct? or rather make it a class?
      else
        None
      end
      match maybe_kind
      | let kind: I64 =>
        try
          let id = module_child(0)?
          if id.id() == TokenIds.tk_id() then
            let name = id.token_value() as String
            (let start_pos, let end_pos) = module_child.span()
            let full_range = LspPositionRange(
              LspPosition.from_ast_pos(start_pos),
              LspPosition.from_ast_pos(end_pos)
            )
            (let id_start, let id_end) = id.span()
            let selection_range = LspPositionRange(
              LspPosition.from_ast_pos(id_start),
              LspPosition.from_ast_pos(id_end)
            )
            let symbol = DocumentSymbol(name, kind, full_range, selection_range)
            this.find_members(module_child, symbol, channel)
            symbols.push(symbol)
          else
            channel.log("Expecred TK_ID, got " + TokenIds.string(id.id()))
          end
        else
          channel.log("No id node at idx 1")
        end
      end
    end
    symbols

  fun tag find_members(entity: AST, symbol: DocumentSymbol ref, channel: Channel) =>
    let members =
      try
        entity(4)?
      else
        channel.log("No members node at child idx 4 for node " + TokenIds.string(entity.id()))
        return
      end
    if members.id() != TokenIds.tk_members() then
      channel.log("Expected members at idx 4, got " + TokenIds.string(members.id()))
      return
    end
      for entity_child in members.children() do
        try
          let maybe_kind_and_idx =
            match entity_child.id()
            | TokenIds.tk_new() => (SymbolKinds.constructor(), USize(1))
            | TokenIds.tk_fun() | TokenIds.tk_be() => (SymbolKinds.method(), USize(1))
            | TokenIds.tk_flet() | TokenIds.tk_fvar() | TokenIds.tk_embed() => (SymbolKinds.field(), USize(0))
            else
              (None, None)
            end
          match maybe_kind_and_idx
          | (let kind: I64, let idx: USize) =>
            let id = entity_child(idx)?
            Fact(id.id() == TokenIds.tk_id(), "Expected TK_ID node for child of " + TokenIds.string(entity_child.id()) +  ", got " + TokenIds.string(id.id()))?
            let name = id.token_value() as String
            (let start_pos, let end_pos) = entity_child.span()
            let full_range = LspPositionRange(
              LspPosition.from_ast_pos(start_pos),
              LspPosition.from_ast_pos(end_pos)
            )
            (let id_start, let id_end) = id.span()
            let selection_range = LspPositionRange(
              LspPosition.from_ast_pos(id_start),
              LspPosition.from_ast_pos(id_end)
            )
            let member_symbol = DocumentSymbol(name, kind, full_range, selection_range)
            symbol.push_child(member_symbol)
          end
        end
    end
