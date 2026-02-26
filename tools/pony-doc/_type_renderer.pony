primitive TypeRenderer
  """
  Renders `DocType` values to markdown strings.

  Matches `doc_type()` (lines 345-444) and `doc_type_list()` (lines 449-484)
  in docgen.c. Handles cross-reference links for nominals, bracket escaping,
  and the `break_lines` parameter for inserting line breaks after every 3rd
  item in a type list.
  """
  fun render(
    doc_type: DocType,
    generate_links: Bool,
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """Render a DocType to a markdown string."""
    match doc_type
    | let n: DocNominal => _nominal(n, generate_links, break_lines,
        include_private)
    | let t: DocTypeParamRef => _type_param_ref(t)
    | let u: DocUnion =>
      _type_list(u.types, "(", " | ", ")", generate_links, break_lines,
        include_private)
    | let i: DocIntersection =>
      _type_list(i.types, "(", " & ", ")", generate_links, break_lines,
        include_private)
    | let t: DocTuple =>
      _type_list(t.types, "(", " , ", ")", generate_links, break_lines,
        include_private)
    | let a: DocArrow => _arrow(a, generate_links, break_lines,
        include_private)
    | let _: DocThis => "this"
    | let c: DocCapability => c.name
    end

  fun render_type_params(
    type_params: Array[DocTypeParam] val,
    generate_links: Bool,
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render type parameters with surrounding brackets.

    Matches `doc_type_params()` in docgen.c (lines 559-601). Uses escaped
    brackets `\[`/`\]` when links are active. Prepends `optional ` when a
    type parameter has a default type.
    """
    if type_params.size() == 0 then return "" end

    let result = recover iso String end
    if generate_links then
      result.append("\\[")
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
        result.append(render(c, generate_links, break_lines, include_private))
      else
        result.append("no constraint")
      end
    end

    if generate_links then
      result.append("\\]")
    else
      result.append("]")
    end
    consume result

  fun render_provides(
    provides: Array[DocType] val,
    preamble: String,
    separator: String,
    postamble: String,
    generate_links: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render a provides/implements type list.

    Used for both the code block provides list (`is\n  ` preamble, no links)
    and the Implements/Type Alias For section (with links, bullet list format).
    """
    _type_list(provides, preamble, separator, postamble,
      generate_links, false, include_private)

  fun _nominal(
    n: DocNominal,
    generate_links: Bool,
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render a nominal type, optionally as a cross-reference link.

    Links are generated when `generate_links` is true AND the type is not
    anonymous (compiler-generated) AND either `include_private` is true or
    the type is not private. This matches docgen.c lines 362-383.
    """
    let result = recover iso String end
    let should_link =
      generate_links
        and (not n.is_anonymous)
        and (include_private or (not n.is_private))

    if should_link then
      result.append("[")
      result.append(n.name)
      result.append("](")
      result.append(n.tqfn)
      result.append(".md)")
      // Type args with escaped brackets and links
      if n.type_args.size() > 0 then
        result.append(
          _type_list(n.type_args, "\\[", ", ", "\\]", true, false,
            include_private))
      end
    else
      result.append(n.name)
      // Type args with plain brackets, no links
      if n.type_args.size() > 0 then
        result.append(
          _type_list(n.type_args, "[", ", ", "]", false, false,
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
    """Render a type parameter reference with optional ephemeral modifier."""
    match t.ephemeral
    | let e: String => t.name + e
    else
      t.name
    end

  fun _arrow(
    a: DocArrow,
    generate_links: Bool,
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """Render an arrow (viewpoint adaptation) type."""
    let left = render(a.left, generate_links, break_lines, include_private)
    let right = render(a.right, generate_links, break_lines, include_private)
    left + "->" + right

  fun _type_list(
    types: Array[DocType] val,
    preamble: String,
    separator: String,
    postamble: String,
    generate_links: Bool,
    break_lines: Bool,
    include_private: Bool)
    : String
  =>
    """
    Render a list of types with preamble, separator, and postamble.

    When `break_lines` is true, inserts `\n    ` after every 3rd separator,
    matching `doc_type_list()` in docgen.c (lines 449-484).
    """
    if types.size() == 0 then return "" end

    let result = recover iso String end
    result.append(preamble)

    var list_item_count: USize = 0
    for (i, t) in types.pairs() do
      result.append(render(t, generate_links, break_lines, include_private))

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
