use ".."
use "json"
use "pony_compiler"

primitive InlayHints
  """
  Collects inlay hints for a module. Three kinds of hints are emitted:
  - Inferred type hints on let/var/field declarations with no type annotation.
  - Capability hints on type annotations where the capability is absent from
    source (including generic type arguments, union, and tuple members).
  - Receiver capability and return type hints on fun declarations.
  """
  fun collect(
    module: Module val,
    range: (None | (I64, I64, I64, I64)) = None): Array[JsonValue]
  =>
    """
    Returns all inlay hints for module, optionally filtered to range
    (start_line, start_char, end_line, end_char) in 0-based LSP coordinates.
    """
    let collector = _InlayHintCollector.create(range)
    module.ast.visit(collector)
    collector.hints()

class ref _InlayHintCollector is ASTVisitor
  """
  AST visitor that collects inlay hints for a single module. Walks
  let/var/field declarations and function nodes, emitting hints for
  inferred types, missing capability annotations, and implicit receiver
  capabilities.
  """
  let _hints: Array[JsonValue] ref
  let _range: (None | (I64, I64, I64, I64))
  var _cursor_line: USize
  var _cursor_line_start: USize

  new ref create(range: (None | (I64, I64, I64, I64)) = None) =>
    _hints = Array[JsonValue].create()
    _range = range
    _cursor_line = 1
    _cursor_line_start = 0

  fun ref visit(node: AST box): VisitResult =>
    """
    Dispatches let/var/field/embed nodes to the type hint handler and fun nodes
    to the return type and receiver cap hint handlers. Behaviours (be) and
    constructors (new) are excluded: be has no receiver cap or return type,
    and new always returns the constructed type which is unambiguous.
    """
    match node.id()
    | TokenIds.tk_let() | TokenIds.tk_var()
    | TokenIds.tk_flet() | TokenIds.tk_fvar() | TokenIds.tk_embed() =>
      _try_add_hint(node)
    | TokenIds.tk_fun() =>
      _try_add_receiver_cap_hint(node)
      _try_add_return_type_hint(node)
    end
    Continue

  fun ref _try_add_hint(node: AST box) =>
    """
    Handles a let/var/field declaration. If the declaration has no type
    annotation, emits a hint showing the full inferred type. If it has a
    partial annotation (missing capability), delegates to _add_nominal_hints.
    """
    try
      let id_node = node(0)?
      if id_node.id() != TokenIds.tk_id() then
        return
      end

      let name = id_node.token_value() as String
      if name.size() == 0 then
        return
      end
      if name(0)? == '$' then
        return
      end

      let line = id_node.line()
      let col = id_node.pos()
      if (line == 0) or (col == 0) then
        return
      end

      let src = id_node.source_contents() as String box
      let after_name = _byte_offset(src, line, col)? + name.size()
      if InlayHintSource.has_annotation(src, after_name) then
        _add_nominal_hints(node(1)?, src)
      else
        let type_str = node.ast_type_string() as String
        let hint_line = (line - 1).i64()
        let hint_char = ((col - 1) + name.size()).i64()
        _push_hint(hint_line, hint_char, ": " + type_str)
      end
    end

  fun ref _add_nominal_hints(type_node: AST box, src: String box) =>
    """
    Recursively walks a type annotation AST and adds capability hints for
    each nominal type that lacks an explicit capability in source.
    The outer nominal is processed before its type args so that
    _byte_offset's forward cursor is never asked to go backward: the outer
    type name is always at an earlier (or equal) source line than its type
    arguments.
    """
    match type_node.id()
    | TokenIds.tk_nominal() =>
      _add_one_nominal_hint(type_node, src)
      try
        let targs = type_node(2)?
        for child in targs.children() do
          if child.id() != TokenIds.tk_none() then
            _add_nominal_hints(child, src)
          end
        end
      end
    | TokenIds.tk_uniontype()
    | TokenIds.tk_isecttype()
    | TokenIds.tk_tupletype() =>
      for child in type_node.children() do
        _add_nominal_hints(child, src)
      end
    end

  fun ref _add_one_nominal_hint(nominal: AST box, src: String box) =>
    """
    Checks whether a tk_nominal type annotation has an explicit capability
    in source. If not, pushes a capability hint right after the type's
    source span (name + optional type args).
    """
    try
      let id_node = nominal(1)?
      if id_node.id() != TokenIds.tk_id() then
        return
      end

      let name = id_node.token_value() as String
      let line = id_node.line()
      let col = id_node.pos()
      if (line == 0) or (col == 0) then
        return
      end

      var j = _byte_offset(src, line, col)? + name.size()
      var j_line = line
      var j_col = col + name.size()

      let targs = nominal(2)?
      var has_targs = false
      for child in targs.children() do
        if child.id() != TokenIds.tk_none() then
          has_targs = true; break
        end
      end

      if has_targs then
        (j, j_line, j_col) =
          InlayHintSource.scan_past_brackets(src, j, j_line, j_col)?
      end

      let hint_j_line = j_line
      let hint_j_col = j_col

      if InlayHintSource.has_explicit_cap_after(src, j) then
        return
      end

      let cap_node = nominal(3)?
      // Skip non-concrete caps (cap sets like #read, #any, or tk_none).
      let cap_id = cap_node.id()
      if not _is_concrete_cap(cap_id) then
        return
      end

      let cap_str = cap_node.get_print()
      _push_hint((hint_j_line - 1).i64(), (hint_j_col - 1).i64(), " " + cap_str)
    end

  fun ref _try_add_return_type_hint(node: AST box) =>
    """
    After typechecking, child(4) always contains the return type (inferred
    or explicit). Source scanning distinguishes the two cases.
    """
    try
      let id_node = node(1)?
      if id_node.id() != TokenIds.tk_id() then
        return
      end

      let name = id_node.token_value() as String
      let line = id_node.line()
      let col = id_node.pos()
      if (line == 0) or (col == 0) then
        return
      end

      let src = id_node.source_contents() as String box
      let start = _byte_offset(src, line, col)? + name.size()
      var hint_line: USize = 0
      var hint_col: USize = 0
      var close_j: USize = 0
      (close_j, hint_line, hint_col) =
        InlayHintSource.scan_to_close_paren(
          src, start, line, col + name.size())?

      if InlayHintSource.explicit_return_type(src, close_j)? then
        _add_nominal_hints(node(4)?, src)
      else
        let type_str = node(4)?.type_string() as String
        // hint_col is the 1-based column after ')'. LSP characters are
        // 0-based, so the position immediately after ')' is hint_col - 1.
        _push_hint(
          (hint_line - 1).i64(),
          (hint_col - 1).i64(),
          ": " + type_str)
      end
    end

  fun ref _try_add_receiver_cap_hint(node: AST box) =>
    """
    Emits a receiver capability hint after 'fun' when no explicit capability
    keyword appears between 'fun' and the function name in source.
    """
    try
      let cap_node = node(0)?
      // Skip non-concrete caps (cap sets like #read, #any, or tk_none).
      let cap_id = cap_node.id()
      if not _is_concrete_cap(cap_id) then
        return
      end
      let id_node = node(1)?
      if id_node.id() != TokenIds.tk_id() then
        return
      end

      let line = id_node.line()
      let col = id_node.pos()
      if (line == 0) or (col < 3) then
        // col < 3 guards the USize subtraction below: col - 2 would underflow
        // for col 0 (no source location) or col 1-2 (impossible in valid Pony).
        return
      end

      let src = id_node.source_contents() as String box
      let name_start = _byte_offset(src, line, col)?
      if InlayHintSource.has_explicit_receiver_cap(src, name_start) then
        return
      end
      // col is the 1-based column of the function name. LSP characters
      // are 0-based, so the space before the name is at col - 2
      // (0-based name start = col - 1; one before that = col - 2).
      _push_hint((line - 1).i64(), (col - 2).i64(), " " + cap_node.get_print())
    end

  fun ref _byte_offset(src: String box, l: USize, col: USize): USize ? =>
    """
    Converts a 1-based (line, col) to a 0-based byte offset, scanning
    forward from the cursor rather than from byte 0 on each call.
    Total cost across all calls during a forward AST walk is O(file_size).
    Precondition: l must be >= _cursor_line. All callers must visit source
    positions in non-decreasing line order; calling with a line earlier than
    the current cursor errors.
    """
    if l < _cursor_line then
      error
    end

    var j = _cursor_line_start
    var j_line = _cursor_line
    while j_line < l do
      j = src.find("\n", j.isize())?.usize() + 1
      j_line = j_line + 1
    end
    _cursor_line = j_line
    _cursor_line_start = j
    j + (col - 1)

  fun ref hints(): Array[JsonValue] =>
    _hints

  fun ref _push_hint(hint_line: I64, hint_char: I64, label: String) =>
    """
    Appends a hint at (hint_line, hint_char) with the given label, skipping
    it if it falls outside _range.
    """
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
        .update("label", label)
        .update("kind", I64(1)))

  fun _is_concrete_cap(id: I32): Bool =>
    """
    Returns true if id is one of the six concrete capability token IDs
    (iso, trn, ref, val, box, tag). Returns false for cap sets (#read,
    #any, etc.) and tk_none.
    """
    (id == TokenIds.tk_iso()) or (id == TokenIds.tk_trn())
      or (id == TokenIds.tk_ref()) or (id == TokenIds.tk_val())
      or (id == TokenIds.tk_box()) or (id == TokenIds.tk_tag())
