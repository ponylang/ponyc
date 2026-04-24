use "assert"

use "pony_compiler"
use "json"

primitive SymbolKinds
  """
  LSP document symbol kind constants.
  """
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
  fun tag function(): I64 => 12
  fun tag variable(): I64 => 13
  fun tag constant(): I64 => 14
  fun tag string(): I64 => 15
  fun tag number(): I64 => 16
  fun tag boolean(): I64 => 17
  fun tag array(): I64 => 8
  fun tag sk_object(): I64 => 19
  fun tag key(): I64 => 20
  fun tag null(): I64 => 21
  fun tag enum_member(): I64 => 22
  fun tag sk_struct(): I64 => 23
  fun tag event(): I64 => 24
  fun tag operator(): I64 => 25
  fun tag type_parameter(): I64 => 26

primitive SymbolTags
  """
  LSP document symbol tag constants.
  """
  fun tag deprecated(): I64 => 1

class DocumentSymbol
  """
  Represents a symbol found in a document (e.g. a class, method, or field).
  """
  let name: String
  let detail: (String | None)
    """
    More detail for this symbol, e.g the signature of a function.
    """
  let kind: I64
    """
    The kind of this symbol.
    """
  let tags: (Array[I64] ref | None)
    """
    Tags for this document symbol. since 3.16.0
    """
  let range: LspPositionRange
    """
    The range enclosing this symbol not including leading/trailing whitespace
    but everything else like comments.
    """
  let selection_range: LspPositionRange
    """
    The range that should be selected and revealed when this symbol is
    being picked, e.g. the name of a function.
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

  fun to_json(): JsonValue =>
    """
    Serialize this symbol to JSON.
    """
    var obj = JsonObject
      .update("name", this.name)
      .update("kind", this.kind)
      .update("range", this.range.to_json())
      .update("selectionRange", this.selection_range.to_json())
    if this.detail isnt None then
      obj = obj.update("detail", detail)
    end
    try
      var json_tags = JsonArray
      for tagg in (this.tags as this->Array[I64]).values() do
        json_tags = json_tags.push(tagg)
      end
      obj = obj.update("tags", json_tags)
    end
    if this.children.size() > 0 then
      var json_children = JsonArray
      for child in this.children.values() do
        json_children = json_children.push(child.to_json())
      end
      obj = obj.update("children", json_children)
    end

    obj

primitive DocumentSymbols
  """
  Extracts document symbols from a compiled module.
  """

  fun tag from_module(
    module: Module,
    channel: Channel): Array[DocumentSymbol] ref
  =>
    """
    Build DocumentSymbol trees for every top-level entity in the module.
    """
    let symbols: Array[DocumentSymbol] ref = Array[DocumentSymbol].create(4)
    // Collect entity start positions for look-ahead. Each entity's range
    // is bounded by the next entity's start — this prevents synthesized
    // constructors (which ponyc positions at the next entity's line) from
    // inflating the entity's span.
    let entity_starts = Array[Position].create()
    for module_child in module.ast.children() do
      match module_child.id()
      | TokenIds.tk_interface()
      | TokenIds.tk_trait()
      | TokenIds.tk_primitive()
      | TokenIds.tk_class()
      | TokenIds.tk_type()
      | TokenIds.tk_actor()
      | TokenIds.tk_struct() => entity_starts.push(module_child.position())
      end
    end
    var entity_idx: USize = 0
    for module_child in module.ast.children() do
      let maybe_kind =
        match module_child.id()
        | TokenIds.tk_interface()
        | TokenIds.tk_trait() => SymbolKinds.sk_interface()
        | TokenIds.tk_primitive()
        | TokenIds.tk_class()
        | TokenIds.tk_type()
        | TokenIds.tk_actor() => SymbolKinds.sk_class()
        | TokenIds.tk_struct() => SymbolKinds.sk_struct()
        else
          None
        end
      match maybe_kind
      | let kind: I64 =>
        let max_pos: (Position | None) =
          try entity_starts(entity_idx + 1)? else None end
        entity_idx = entity_idx + 1
        try
          let id = module_child(0)?
          if id.id() == TokenIds.tk_id() then
            let name = id.token_value() as String
            (let full_range, let selection_range) =
              this._symbol_ranges(module_child, id, name, channel, max_pos)?
            let symbol =
              DocumentSymbol(name, kind, full_range, selection_range)
            this.find_members(module_child, symbol, channel, max_pos)
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

  fun tag find_members(
    entity: AST,
    symbol: DocumentSymbol ref,
    channel: Channel,
    max_pos: (Position | None) = None)
  =>
    """
    Walk the members of `entity` and append a child DocumentSymbol for each
    explicitly written member (field, constructor, function, or behaviour).

    Two categories of members are filtered out:
    - Synthesized constructors: ponyc's sugar pass places the default `create`
      at the entity keyword's own source position (via token_dup). Any
      explicitly written member must appear after the entity keyword, so
      members at or before that position are skipped.
    - Trait-merged methods: ponyc merges trait default method bodies into
      implementing classes; those merged nodes carry the trait file's
      source_file(). Members whose source_file() is a non-None string that
      does not match the entity's source_file() are skipped.
    """
    let members =
      try
        entity(4)?
      else
        channel.log(
          "No members node at child idx 4 for node " +
          TokenIds.string(entity.id()))
        return
      end
    if members.id() != TokenIds.tk_members() then
      channel.log(
        "Expected members at idx 4, got " +
        TokenIds.string(members.id()))
      return
    end
    let doc_path = entity.source_file()
    let entity_pos = entity.position()
    for entity_child in members.children() do
      // Skip members whose position is at or beyond max_pos.
      match max_pos
      | let m: Position =>
        if entity_child.position() >= m then continue end
      end
      // Skip members at or before the entity's keyword position.
      // ponyc places synthesized constructors (the default `create`
      // added to bare classes/actors/primitives/structs) at the entity
      // keyword's own source position via token_dup(). Any explicitly
      // written member must appear after the entity keyword.
      if entity_child.position() <= entity_pos then continue end
      // Skip members from other source files. ponyc merges trait
      // default method bodies into implementing classes; those merged
      // nodes carry the trait file's source_file(). Nodes with
      // source_file() == None are synthetic containers and are kept.
      match doc_path
      | let dp: String val =>
        match entity_child.source_file()
        | let sf: String val =>
          if sf != dp then continue end
        end
      end
      try
        let maybe_kind_and_idx =
          match entity_child.id()
          | TokenIds.tk_new() => (SymbolKinds.constructor(), USize(1))
          | TokenIds.tk_fun()
          | TokenIds.tk_be() => (SymbolKinds.method(), USize(1))
          | TokenIds.tk_flet()
          | TokenIds.tk_fvar()
          | TokenIds.tk_embed() => (SymbolKinds.field(), USize(0))
          else
            (None, None)
          end
        match maybe_kind_and_idx
        | (let kind: I64, let idx: USize) =>
          let id = entity_child(idx)?
          Fact(
            id.id() == TokenIds.tk_id(),
            "Expected TK_ID node for child of " +
            TokenIds.string(entity_child.id()) +
            ", got " + TokenIds.string(id.id()))?
          let name = id.token_value() as String
          (let full_range, let selection_range) =
            this._symbol_ranges(entity_child, id, name, channel, max_pos)?
          let member_symbol =
            DocumentSymbol(name, kind, full_range, selection_range)
          symbol.push_child(member_symbol)
        end
      end
    end

  fun tag _symbol_ranges(
    node: AST box,
    id: AST box,
    name: String,
    channel: Channel,
    max_pos: (Position | None) = None)
    : (LspPositionRange, LspPositionRange) ?
  =>
    """
    Compute the full declaration range and the identifier selection range
    for an entity or member AST node. Raises error (logging to `channel`)
    if `source_file()` is absent or if `ASTClampedRange` returns an inverted
    span — callers inside a `try` block will skip the symbol on failure.
    """
    let doc_path =
      try
        node.source_file() as String val
      else
        channel.log(
          "No source_file for " +
          TokenIds.string(node.id()) + " '" + name + "'")
        error
      end
    let full_range =
      match \exhaustive\ ASTClampedRange(node, doc_path, max_pos)
      | let r: LspPositionRange => r
      | None =>
        channel.log(
          "Inverted source span for " +
          TokenIds.string(node.id()) + " '" + name + "'")
        error
      end
    (let id_start, let id_end) = id.span()
    let selection_range =
      LspPositionRange(
        LspPosition.from_ast_pos(id_start),
        LspPosition.from_ast_pos_end(id_end))
    (full_range, selection_range)
