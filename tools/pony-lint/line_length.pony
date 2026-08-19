use "collections"
use ast = "pony_compiler"

primitive LineLength is ASTRule
  """
  Flags lines exceeding 80 codepoints.

  Column is reported as 81 (the first character beyond the limit). The count
  uses `String.codepoints()` so multi-byte UTF-8 characters count as one
  codepoint each. Note that compound emoji (e.g., flag sequences) may count
  as multiple codepoints.

  Lines where one of the first two space-delimited words crosses column 80
  are exempt. A word at position 1 or 2 that starts at or before column 80
  and extends past it is treated as unbreakable — breaking the line cannot
  bring the word under 80 columns given realistic indentation. Words deeper
  in the line (position 3+) can be moved to their own line, so they do not
  earn an exemption.

  Lines inside triple-quoted string literals (non-docstring `\"\"\"` blocks)
  are exempt from the 80-column check. Triple-quoted strings used as data
  (JSON, inline test fixtures, etc.) exist for readability; forcing them to
  wrap defeats their purpose. Lines inside docstrings are not exempt —
  docstring prose should be wrapped at 80 columns, though the word rule
  above still applies.
  """
  fun id(): String val => "style/line-length"
  fun category(): String val => "style"
  fun description(): String val => "line exceeds 80 columns"
  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    recover val Array[ast.TokenId] end

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    recover val Array[Diagnostic val] end

  fun check_module(module_ast: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Uses the module AST to identify triple-quoted string literals,
    then checks lines for the 80-column limit. String literal content
    lines are fully exempt.
    """
    let visitor = _StringLiteralVisitor(source)
    module_ast.visit(visitor)
    let exempt_lines = _collect_lines(visitor.literal_ranges())
    check_text(source, exempt_lines)

  fun check_text(
    source: SourceFile val,
    exempt_lines: Set[USize] val = recover val Set[USize] end)
    : Array[Diagnostic val] val
  =>
    recover val
      let result = Array[Diagnostic val]
      for (idx, line) in source.lines.pairs() do
        let line_no = idx + 1
        let cp_count = line.codepoints()
        if cp_count > 80 then
          if exempt_lines.contains(line_no) then
            continue
          end
          if not _has_exempt_word(line) then
            result.push(Diagnostic(
              id(),
              "line exceeds 80 columns (" + cp_count.string() + ")",
              source.rel_path,
              line_no,
              81))
          end
        end
      end
      result
    end

  fun _collect_lines(ranges: Array[(USize, USize)] val): Set[USize] val =>
    let lines = recover iso Set[USize] end
    for (start_line, end_line) in ranges.values() do
      var l = start_line
      while l <= end_line do
        lines.set(l)
        l = l + 1
      end
    end
    consume lines

  fun _has_exempt_word(line: String val): Bool =>
    """
    True when one of the first two space-delimited words on the line
    crosses column 80 — starts at or before column 80 and extends past
    it. Only the first two words qualify because a long word later on
    the line can be moved to its own line where it would be exempt.
    """
    var in_word = false
    var i: USize = 0
    let size = line.size()
    var cp_pos: USize = 0
    var word_start_cp: USize = 0
    var word_count: USize = 0

    while i < size do
      try
        let byte = line(i)?

        if (byte and 0xC0) != 0x80 then
          cp_pos = cp_pos + 1
        end

        if byte == ' ' then
          if in_word and (word_start_cp <= 80)
            and ((cp_pos - 1) > 80)
          then
            return true
          end
          in_word = false
        elseif not in_word then
          word_count = word_count + 1
          if word_count > 2 then return false end
          word_start_cp = cp_pos
          in_word = true
        end
      else
        _Unreachable()
      end
      i = i + 1
    end

    in_word and (word_count <= 2)
      and (word_start_cp <= 80) and (cp_pos > 80)

class ref _StringLiteralVisitor is ast.ASTVisitor
  """
  Walks the AST collecting line ranges of non-docstring triple-quoted
  string literals. Docstrings are identified and excluded — their
  content lines go through the normal line-length check.
  """
  let _source: SourceFile val
  let _literal_ranges: Array[(USize, USize)]

  new ref create(source: SourceFile val) =>
    _source = source
    _literal_ranges = Array[(USize, USize)]

  fun ref visit(node: ast.AST box): ast.VisitResult =>
    if node.id() != ast.TokenIds.tk_string() then
      return ast.Continue
    end

    if not _is_triple_quoted(node) then
      return ast.Continue
    end

    let start_line = node.line()
    let end_line =
      match node.end_pos()
      | let p: ast.Position => p.line()
      else
        start_line
      end
    if (end_line > start_line) and (not _is_docstring(node)) then
      _literal_ranges.push((start_line, end_line))
    end
    ast.Continue

  fun literal_ranges(): Array[(USize, USize)] val =>
    let result = recover iso Array[(USize, USize)] end
    for r in _literal_ranges.values() do
      result.push(r)
    end
    consume result

  fun _is_docstring(node: ast.AST box): Bool =>
    """
    A TK_STRING is a docstring when it occupies one of these positions:
    - Child 6 of an entity (class, actor, primitive, struct, trait,
      interface)
    - Child 0 of a method body (first expression in the TK_SEQ that is
      child 6 of a tk_fun/tk_new/tk_be)
    - Child 7 of a method (abstract method docstring slot)
    - First child of a module (package-level docstring)
    """
    let parent =
      match node.parent()
      | let p: ast.AST => p
      else
        return false
      end

    let idx = node.sibling_idx()
    let parent_id = parent.id()

    // Module-level docstring
    if (parent_id == ast.TokenIds.tk_module()) and (idx == 0) then
      return true
    end

    // Entity docstring at child 6
    if ast.TokenIds.is_entity(parent_id) and (idx == 6) then
      return true
    end

    // Method docstrings
    let is_method =
      (parent_id == ast.TokenIds.tk_fun())
        or (parent_id == ast.TokenIds.tk_new())
        or (parent_id == ast.TokenIds.tk_be())

    // Abstract method docstring at child 7
    if is_method and (idx == 7) then
      return true
    end

    // Concrete method docstring: first child of body TK_SEQ
    if (parent_id == ast.TokenIds.tk_seq()) and (idx == 0) then
      match parent.parent()
      | let grandparent: ast.AST =>
        let gp_id = grandparent.id()
        if (gp_id == ast.TokenIds.tk_fun())
          or (gp_id == ast.TokenIds.tk_new())
          or (gp_id == ast.TokenIds.tk_be())
        then
          return true
        end
      end
    end

    false

  fun _is_triple_quoted(node: ast.AST box): Bool =>
    """
    Check whether the TK_STRING at the given position in the source is
    triple-quoted by examining the source text at the node's start
    position.
    """
    let l = node.line()
    let col = node.pos()
    if (l == 0) or (col == 0) then return false end
    try
      let line_text = _source.lines(l - 1)?
      let byte_offset = col - 1
      if (byte_offset + 2) < line_text.size() then
        (line_text(byte_offset)? == '"')
          and (line_text(byte_offset + 1)? == '"')
          and (line_text(byte_offset + 2)? == '"')
      else
        false
      end
    else
      false
    end
