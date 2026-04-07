primitive TypeRenderer
  """
  Renders `DocType` values to format-agnostic strings.

  Link generation and bracket escaping are delegated to a `LinkFormat`
  implementation. Pass `None` for plain-text output (no links, plain
  brackets).
  """
  fun render(
    doc_type: DocType,
    format: (LinkFormat val | None),
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render a DocType to a string.

    When `format` is a `LinkFormat`, cross-reference links and escaped
    brackets are produced according to that format. When `None`, the
    output is plain text with no links or bracket escaping.
    """
    match \exhaustive\ doc_type
    | let n: DocNominal =>
      _nominal(
        n,
        format,
        break_lines,
        include_private)
    | let t: DocTypeParamRef => _type_param_ref(t)
    | let u: DocUnion =>
      _type_list(
        u.types,
        "(",
        " | ",
        ")",
        format,
        break_lines,
        include_private)
    | let i: DocIntersection =>
      _type_list(
        i.types,
        "(",
        " & ",
        ")",
        format,
        break_lines,
        include_private)
    | let t: DocTuple =>
      _type_list(
        t.types,
        "(",
        " , ",
        ")",
        format,
        break_lines,
        include_private)
    | let a: DocArrow =>
      _arrow(
        a,
        format,
        break_lines,
        include_private)
    | let _: DocThis => "this"
    | let c: DocCapability => c.name
    end

  fun render_type_params(
    type_params: Array[DocTypeParam] val,
    format: (LinkFormat val | None),
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render type parameters with surrounding brackets.

    Uses format-specific brackets when a `LinkFormat` is provided, plain
    `[`/`]` otherwise. Prepends `optional ` when a type parameter has a
    default type.
    """
    if type_params.size() == 0 then return "" end

    let result = recover iso String end
    match format
    | let f: LinkFormat =>
      result.append(f.open_bracket())
    else
      result.append("[")
    end

    for (i, tp) in type_params.pairs() do
      if i > 0 then result.append(", ") end

      match tp.default_type
      | let _: DocType => result.append("optional ")
      end

      result.append(tp.name)
      result.append(": ")

      match tp.constraint
      | let c: DocType =>
        result.append(render(c, format, break_lines, include_private))
      else
        result.append("no constraint")
      end
    end

    match format
    | let f: LinkFormat =>
      result.append(f.close_bracket())
    else
      result.append("]")
    end
    consume result

  fun render_provides(
    provides: Array[DocType] val,
    preamble: String,
    separator: String,
    postamble: String,
    format: (LinkFormat val | None),
    include_private: Bool)
    : String
  =>
    """
    Render a provides/implements type list.

    Used for both the code block provides list (no format) and the
    Implements/Type Alias For section (with a `LinkFormat`, bullet list
    format).
    """
    _type_list(
      provides,
      preamble,
      separator,
      postamble,
      format,
      false,
      include_private)

  fun _nominal(
    n: DocNominal,
    format: (LinkFormat val | None),
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render a nominal type, optionally as a cross-reference link.

    A link is generated when `format` provides a `LinkFormat` AND the type
    is not anonymous (compiler-generated) AND either `include_private` is
    true or the type is not private.
    """
    let result = recover iso String end

    match format
    | let f: LinkFormat if
        (not n.is_anonymous)
          and (include_private or (not n.is_private))
    =>
      result.append(f.link(n.name, n.tqfn))
      // Type args with format-specific brackets and links
      if n.type_args.size() > 0 then
        result.append(
          _type_list(
            n.type_args,
            f.open_bracket(),
            ", ",
            f.close_bracket(),
            format,
            false,
            include_private))
      end
    else
      // When the nominal is not linkable (no format, anonymous, or private
      // without include_private), type args are also rendered without links.
      result.append(n.name)
      // Type args with plain brackets, no links
      if n.type_args.size() > 0 then
        result.append(
          _type_list(
            n.type_args,
            "[",
            ", ",
            "]",
            None,
            false,
            include_private))
      end
    end

    match n.cap
    | let c: String =>
      result.append(" ")
      result.append(c)
    end

    match n.ephemeral
    | let e: String => result.append(e)
    end

    consume result

  fun _type_param_ref(t: DocTypeParamRef): String =>
    """
    Render a type parameter reference with optional
    ephemeral modifier.
    """
    match t.ephemeral
    | let e: String => t.name + e
    else
      t.name
    end

  fun _arrow(
    a: DocArrow,
    format: (LinkFormat val | None),
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render an arrow (viewpoint adaptation) type.
    """
    let left = render(a.left, format, break_lines, include_private)
    let right = render(a.right, format, break_lines, include_private)
    left + "->" + right

  fun _type_list(
    types: Array[DocType] val,
    preamble: String,
    separator: String,
    postamble: String,
    format: (LinkFormat val | None),
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render a list of types with preamble, separator, and postamble.

    When `break_lines` is true, inserts `\n    ` after every 3rd separator.
    """
    if types.size() == 0 then return "" end

    let result = recover iso String end
    result.append(preamble)

    var list_item_count: USize = 0
    for (i, t) in types.pairs() do
      result.append(render(t, format, break_lines, include_private))

      if i < (types.size() - 1) then
        result.append(separator)

        if break_lines then
          if list_item_count == 2 then
            result.append("\n    ")
            list_item_count = 0
          else
            list_item_count = list_item_count + 1
          end
        end
      end
    end

    result.append(postamble)
    consume result
