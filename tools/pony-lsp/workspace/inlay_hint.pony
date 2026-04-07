use ".."
use "json"
use "pony_compiler"

primitive InlayHints
  """
  Collects inlay hints for a module: type hints for let/var declarations
  that have no explicit type annotation.
  """
  fun collect(
    module: Module val,
    range: (None | (I64, I64, I64, I64)) = None)
    : Array[JsonValue]
  =>
    let collector = _InlayHintCollector.create(range)
    module.ast.visit(collector)
    collector.hints()

class ref _InlayHintCollector is ASTVisitor
  let _hints: Array[JsonValue] ref
  let _range: (None | (I64, I64, I64, I64))

  new ref create(range: (None | (I64, I64, I64, I64)) = None) =>
    _hints = Array[JsonValue].create()
    _range = range

  fun ref visit(node: AST box): VisitResult =>
    match node.id()
    | TokenIds.tk_let() | TokenIds.tk_var() =>
      _try_add_hint(node)
    end
    Continue

  fun ref _try_add_hint(node: AST box) =>
    try
      let id_node = node(0)?
      if id_node.id() != TokenIds.tk_id() then
        return
      end
      let name = id_node.token_value() as String
      let l = id_node.line()
      let col = id_node.pos()
      if (l == 0) or (col == 0) then
        return // synthetic node
      end
      if _has_explicit_type(id_node, name, l, col) then
        return
      end
      let type_str = node.ast_type_string() as String
      // LSP positions are 0-based; AST positions are 1-based
      let hint_line = (l - 1).i64()
      let hint_char = ((col - 1) + name.size()).i64()
      match _range
      | (let sl: I64, let sc: I64, let el: I64, let ec: I64) =>
        if hint_line < sl then return end
        if hint_line > el then return end
        if (hint_line == sl) and (hint_char < sc) then return end
        if (hint_line == el) and (hint_char >= ec) then return end
      end
      _hints.push(
        JsonObject
          .update(
            "position",
            JsonObject
              .update("line", hint_line)
              .update("character", hint_char))
          .update("label", ": " + type_str)
          .update("kind", I64(1)))
    end

  fun ref hints(): Array[JsonValue] =>
    _hints

  fun _has_explicit_type(
    id_node: AST box,
    name: String,
    l: USize,
    col: USize)
    : Bool
  =>
    """
    Scan source text to detect whether the let/var declaration has an
    explicit type annotation (a ':' between the name and '=').
    Callers guarantee l >= 1 and col >= 1.
    """
    try
      let src = id_node.source_contents() as String box
      // Find byte offset of the first character of the identifier
      let base_offset: USize =
        if l == 1 then
          col - 1
        else
          let line_idx = src.find("\n" where nth = l - 2)?
          USize(line_idx.usize() + col)
        end
      // Scan from the character right after the identifier
      var j = base_offset + name.size()
      while j < src.size() do
        match src(j)?
        | ':' => return true  // explicit type annotation
        | '=' | '\n' => return false // no annotation
        | ' ' | '\t' => None // skip whitespace
        else
          return false // unexpected character
        end
        j = j + 1
      end
      false
    else
      false
    end
