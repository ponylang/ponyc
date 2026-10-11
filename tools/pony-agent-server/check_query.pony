use "pony_compiler"

class val CheckResult
  """
  Whether a source type is a subtype of a target type, with an
  explanation when it is not and suggestions for making it compatible.
  """
  let compatible: Bool
  let source_type: String val
  let target_type: String val
  let reason: String val
  let suggestions: Array[String val] val

  new val create(
    compatible': Bool,
    source_type': String val,
    target_type': String val,
    reason': String val,
    suggestions': Array[String val] val)
  =>
    compatible = compatible'
    source_type = source_type'
    target_type = target_type'
    reason = reason'
    suggestions = suggestions'

primitive CheckQuery
  """
  Check whether one type is a subtype of another using the compiler's
  own subtype checker. Accepts type names with capabilities (e.g.
  "String val", "Stringable ref") and returns compatibility with an
  explanation.
  """

  fun apply(
    session: CompileSession box,
    source: String val,
    target: String val)
    : (CheckResult val | String val)
  =>
    """
    Check whether `source` is a subtype of `target`. Both are type
    names optionally followed by a capability ("String val",
    "Collector ref"). When no capability is given, the type's default
    is used.
    """
    let program =
      match \exhaustive\ session.program()
      | let p: Program val => p
      | None => return "no compiled program available"
      end

    (let source_name, let source_cap) = _parse_type(source)
    (let target_name, let target_cap) = _parse_type(target)

    if source_name.size() == 0 then
      return "cannot parse source type: " + source
    end
    if target_name.size() == 0 then
      return "cannot parse target type: " + target
    end

    let source_def =
      match \exhaustive\ ASTHelpers.find_type_def(program, source_name)
      | let t: AST box => t
      | let err: String val => return err
      | None => return "source type not found: " + source_name
      end

    let target_def =
      match \exhaustive\ ASTHelpers.find_type_def(program, target_name)
      | let t: AST box => t
      | let err: String val => return err
      | None => return "target type not found: " + target_name
      end

    let source_cap_id = _cap_token_id(source_cap, source_def)
    let target_cap_id = _cap_token_id(target_cap, target_def)

    let is_sub =
      session.is_subtype(
        source_def, source_cap_id, target_def, target_cap_id)

    let source_display: String val =
      source_name + " " + _cap_name(source_cap_id)
    let target_display: String val =
      target_name + " " + _cap_name(target_cap_id)

    if is_sub then
      let reason: String val =
        source_display + " is a subtype of " + target_display
      CheckResult(
        true,
        source_display,
        target_display,
        reason,
        recover val Array[String val] end)
    else
      let reason: String val =
        source_display + " is not a subtype of " + target_display
      let suggestions =
        _suggest(
          source_name,
          source_cap,
          source_def,
          target_name,
          target_cap,
          target_def,
          session)
      CheckResult(false, source_display, target_display, reason, suggestions)
    end

  fun _parse_type(type_str: String val): (String val, String val) =>
    """
    Split "TypeName cap" into (name, cap). Returns ("TypeName", "")
    when no capability is given.
    """
    try
      let space = type_str.rfind(" ")?
      let name: String val = type_str.substring(0, space)
      let cap: String val = type_str.substring(space + 1)
      if _is_cap(cap) then
        return (name, cap)
      end
    end
    (type_str, "")

  fun _is_cap(s: String val): Bool =>
    match s
    | "iso" | "trn" | "ref" | "val" | "box" | "tag" => true
    else false
    end

  fun _cap_token_id(cap: String val, def: AST box): TokenId =>
    """
    Token ID for a capability name. When `cap` is empty, returns the
    type's default capability based on its entity kind.
    """
    match cap
    | "iso" => TokenIds.tk_iso()
    | "trn" => TokenIds.tk_trn()
    | "ref" => TokenIds.tk_ref()
    | "val" => TokenIds.tk_val()
    | "box" => TokenIds.tk_box()
    | "tag" => TokenIds.tk_tag()
    else ASTHelpers.default_cap(def)
    end

  fun _cap_name(token_id: TokenId): String val =>
    CapSubtyping.cap_string(token_id)

  fun _suggest(
    source_name: String val,
    source_cap: String val,
    source_def: AST box,
    target_name: String val,
    target_cap: String val,
    target_def: AST box,
    session: CompileSession box)
    : Array[String val] val
  =>
    let suggestions = Array[String val]

    let source_cap_id = _cap_token_id(source_cap, source_def)
    let target_cap_id = _cap_token_id(target_cap, target_def)

    if source_name == target_name then
      if not CapSubtyping.is_sub(
        _cap_name(source_cap_id), _cap_name(target_cap_id))
      then
        let sc = _cap_name(source_cap_id)
        let tc = _cap_name(target_cap_id)
        if (tc == "val") or (tc == "iso") then
          let s: String val =
            "Create the value inside a recover " + tc + " block"
          suggestions.push(s)
        end
        if (sc == "ref") and (tc == "val") then
          suggestions.push(
            "Use consume on an iso reference to get val")
        end
        let s: String val =
          "Change the target to accept " + sc + " or box"
        suggestions.push(s)
      end
    else
      let target_kind = target_def.id()
      if (target_kind == TokenIds.tk_trait()) or
        (target_kind == TokenIds.tk_interface())
      then
        let source_at_ref =
          session.is_subtype(
            source_def,
            TokenIds.tk_ref(),
            target_def,
            TokenIds.tk_ref())
        if source_at_ref then
          let s: String val =
            source_name + " satisfies " + target_name +
            " structurally but the capabilities are incompatible"
          suggestions.push(s)
        else
          let s: String val =
            source_name + " does not implement " + target_name
          suggestions.push(s)
        end
      end
    end

    let out = recover iso Array[String val](suggestions.size()) end
    for s in suggestions.values() do out.push(s) end
    consume out

