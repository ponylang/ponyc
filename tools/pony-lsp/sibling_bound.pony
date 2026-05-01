use "pony_compiler"

primitive SiblingBound
  """
  Returns the start position of the AST sibling that immediately follows `n`
  within `n`'s parent's children list, or None if `n` is the last child or
  has no parent.

  Pass as `max_pos` to `ASTSourceSpan` to cap span computation at the next
  sibling's start. This prevents synthesized-constructor end-bleed: ponyc
  positions synthesized default constructors at the start of the next entity,
  so capping at the sibling start excludes those tokens from the current
  node's computed span.
  """
  fun apply(n: AST box): (Position | None) =>
    let n_source =
      match \exhaustive\ n.source_file()
      | let sf: String val => sf
      | None => return None
      end
    match n.parent()
    | let parent: AST box =>
      var past_n = false
      for child in parent.children() do
        if child == n then
          past_n = true
        elseif past_n then
          return _sibling_pos(child, n, n_source)
        end
      end
    end
    None

  fun _sibling_pos(
    child: AST box,
    n: AST box,
    n_source: String val)
    : (Position | None)
  =>
    """
    Returns `child.position()` if `child` is from the same source file as `n`
    and its position strictly follows `n`'s position, otherwise None.
    """
    let child_pos = child.position()
    if child_pos <= n.position() then
      return None
    end
    match \exhaustive\ child.source_file()
    | let sf: String val if sf == n_source => child_pos
    | let _: (String val | None) => None
    end
