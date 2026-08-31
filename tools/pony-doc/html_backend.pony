use "files"

primitive HtmlBackend is Backend
  """
  Generates self-contained HTML documentation output.

  Produces a directory of static HTML files viewable directly in a browser
  with no external dependencies. Each entity page includes a table of
  contents listing members with their full signatures.
  """
  fun generate(
    program: DocProgram box,
    output_dir: FilePath,
    include_private: Bool)
    ?
  =>
    """
    Write HTML files to `output_dir/<program-name>-docs/`.
    """
    let base_name: String val = program.name + "-docs"
    let base_dir = output_dir.join(base_name)?
    base_dir.mkdir()
    let src_dir = base_dir.join("src")?
    src_dir.mkdir()
    let assets_dir = base_dir.join("assets")?
    assets_dir.mkdir()

    let home_links = recover iso String(1024) end
    let written_sources = Array[String]

    for pkg in program.packages.values() do
      let pkg_tqfn = TQFN(pkg.qualified_name, "-index")
      let sanitized_pkg =
        PathSanitize.replace_path_separator(pkg.qualified_name)

      home_links.append("<li><a href=\"")
      home_links.append(HtmlEscape.attribute(pkg_tqfn))
      home_links.append(".html\">")
      home_links.append(HtmlEscape.content(pkg.qualified_name))
      home_links.append("</a></li>\n")

      let public_type_links = recover iso String(512) end
      let private_type_links = recover iso String(512) end

      for entity in pkg.public_types.values() do
        public_type_links.append("<li><a href=\"")
        public_type_links.append(HtmlEscape.attribute(entity.tqfn))
        public_type_links.append(".html\">")
        public_type_links.append(HtmlEscape.content(
          entity.kind.keyword() + " " + entity.name))
        public_type_links.append("</a></li>\n")

        _write_entity_page(
          base_dir, entity, sanitized_pkg, program.name,
          pkg.qualified_name, include_private)?
      end

      for entity in pkg.private_types.values() do
        private_type_links.append("<li><a href=\"")
        private_type_links.append(HtmlEscape.attribute(entity.tqfn))
        private_type_links.append(".html\">")
        private_type_links.append(HtmlEscape.content(
          entity.kind.keyword() + " " + entity.name))
        private_type_links.append("</a></li>\n")

        _write_entity_page(
          base_dir, entity, sanitized_pkg, program.name,
          pkg.qualified_name, include_private)?
      end

      _write_package_page(
        base_dir, pkg, pkg_tqfn, program.name,
        consume public_type_links, consume private_type_links,
        include_private)?

      for sf in pkg.source_files.values() do
        let source_key: String val = sanitized_pkg + "/" + sf.filename
        var already_written = false
        for written in written_sources.values() do
          if written == source_key then
            already_written = true
            break
          end
        end
        if not already_written then
          written_sources.push(source_key)
          let pkg_src_dir = src_dir.join(sanitized_pkg)?
          pkg_src_dir.mkdir()
          _write_source_page(
            src_dir, sanitized_pkg, sf, program.name)?
        end
      end
    end

    _write_home_page(base_dir, program.name, consume home_links)?
    _write_assets(assets_dir)?

  fun _html_page(
    title: String,
    site_name: String,
    breadcrumb: String,
    content: String val,
    root_prefix: String = "")
    : String val
  =>
    let result = recover iso String(content.size() + 1024) end
    result.append("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n")
    result.append("<meta charset=\"utf-8\">\n")
    result.append(
      "<meta name=\"viewport\" " +
        "content=\"width=device-width, initial-scale=1\">\n")
    result.append("<title>")
    result.append(HtmlEscape.content(title))
    result.append(" - ")
    result.append(HtmlEscape.content(site_name))
    result.append("</title>\n")
    result.append("<link rel=\"stylesheet\" href=\"")
    result.append(root_prefix)
    result.append("assets/style.css\">\n")
    result.append("</head>\n<body>\n")

    result.append("<div class=\"header\">\n")
    result.append("<a href=\"")
    result.append(root_prefix)
    result.append("index.html\">")
    result.append(HtmlEscape.content(site_name))
    result.append("</a>\n</div>\n")

    if breadcrumb.size() > 0 then
      result.append("<div class=\"breadcrumb\">")
      result.append(breadcrumb)
      result.append("</div>\n")
    end

    result.append("<main>\n")
    result.append(content)
    result.append("\n</main>\n</body>\n</html>\n")
    consume result

  fun _write_home_page(
    base_dir: FilePath,
    site_name: String,
    links: String val)
    ?
  =>
    let content = recover iso String(512) end
    content.append("<h1>Packages</h1>\n")
    content.append("<ul class=\"type-list\">\n")
    content.append(links)
    content.append("</ul>\n")

    let page = _html_page(
      "Packages", site_name, "", consume content)
    _write_file(base_dir, "index.html", page)?

  fun _write_package_page(
    base_dir: FilePath,
    pkg: DocPackage box,
    pkg_tqfn: String,
    site_name: String,
    public_type_links: String val,
    private_type_links: String val,
    include_private: Bool)
    ?
  =>
    let content = recover iso String(1024) end

    content.append("<h1>")
    content.append(HtmlEscape.content(pkg.qualified_name))
    content.append("</h1>\n")

    match pkg.doc_string
    | let ds: String =>
      content.append("<p>")
      content.append(HtmlEscape.content(ds))
      content.append("</p>\n")
    else
      content.append("<p>No package doc string provided for ")
      content.append(HtmlEscape.content(pkg.qualified_name))
      content.append(".</p>\n")
    end

    if public_type_links.size() > 0 then
      content.append("<h2>Public Types</h2>\n")
      content.append("<ul class=\"type-list\">\n")
      content.append(public_type_links)
      content.append("</ul>\n")
    end

    if include_private and (private_type_links.size() > 0) then
      content.append("<h2>Private Types</h2>\n")
      content.append("<ul class=\"type-list\">\n")
      content.append(private_type_links)
      content.append("</ul>\n")
    end

    let breadcrumb = "<a href=\"index.html\">Packages</a>"
    let page = _html_page(
      pkg.qualified_name, site_name, breadcrumb, consume content)
    _write_file(base_dir, pkg_tqfn + ".html", page)?

  fun _write_entity_page(
    base_dir: FilePath,
    entity: DocEntity box,
    sanitized_pkg: String,
    site_name: String,
    pkg_qualified_name: String,
    include_private: Bool)
    ?
  =>
    let content = recover iso String(4096) end
    let pkg_tqfn = TQFN(pkg_qualified_name, "-index")

    content.append("<h1>")
    content.append(HtmlEscape.content(entity.name))
    content.append(HtmlEscape.content(
      TypeRenderer.render_type_params(
        entity.type_params, None, false, include_private)))
    content.append(
      _source_link(entity.source, sanitized_pkg))
    content.append("</h1>\n")

    match entity.doc_string
    | let ds: String =>
      content.append("<p>")
      content.append(HtmlEscape.content(ds))
      content.append("</p>\n")
    end

    content.append("<pre><code>")
    content.append(HtmlEscape.content(
      entity.kind.keyword() + " "))
    match entity.cap
    | let c: String =>
      content.append(HtmlEscape.content(c + " "))
    end
    content.append(HtmlEscape.content(entity.name))
    content.append(HtmlEscape.content(
      TypeRenderer.render_type_params(
        entity.type_params, None, true, include_private)))
    content.append(HtmlEscape.content(
      TypeRenderer.render_provides(
        entity.provides, " is\n  ", ",\n  ", "", None, include_private)))
    content.append("</code></pre>\n")

    match entity.kind
    | EntityTypeAlias =>
      content.append(
        _render_provides_section(
          entity.provides, "Type Alias For", include_private))
    else
      content.append(
        _render_provides_section(
          entity.provides, "Implements", include_private))
    end

    content.append(
      _render_toc(entity, include_private))

    content.append(
      _render_methods(
        entity.constructors, "Constructors",
        sanitized_pkg, include_private))
    content.append(
      _render_fields(
        entity.public_fields, "Public Fields",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(
        entity.public_behaviours, "Public Behaviours",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(
        entity.public_functions, "Public Functions",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(
        entity.private_behaviours, "Private Behaviours",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(
        entity.private_functions, "Private Functions",
        sanitized_pkg, include_private))

    let breadcrumb: String val =
      "<a href=\"index.html\">Packages</a> &raquo; " +
        "<a href=\"" + HtmlEscape.attribute(pkg_tqfn) + ".html\">" +
        HtmlEscape.content(pkg_qualified_name) + "</a>"
    let page = _html_page(
      entity.name, site_name, breadcrumb, consume content)
    _write_file(base_dir, entity.tqfn + ".html", page)?

  fun _render_toc(
    entity: DocEntity box,
    include_private: Bool)
    : String val
  =>
    if (entity.constructors.size() == 0) and
      (entity.public_fields.size() == 0) and
      (entity.public_behaviours.size() == 0) and
      (entity.public_functions.size() == 0) and
      (entity.private_behaviours.size() == 0) and
      (entity.private_functions.size() == 0)
    then
      return ""
    end

    let result = recover iso String(2048) end
    result.append("<nav class=\"toc\">\n")
    result.append("<h2>Members</h2>\n")

    result.append(
      _render_toc_methods(
        entity.constructors, "Constructors", include_private))
    result.append(
      _render_toc_fields(entity.public_fields, "Public Fields"))
    result.append(
      _render_toc_methods(
        entity.public_behaviours, "Public Behaviours", include_private))
    result.append(
      _render_toc_methods(
        entity.public_functions, "Public Functions", include_private))
    result.append(
      _render_toc_methods(
        entity.private_behaviours, "Private Behaviours", include_private))
    result.append(
      _render_toc_methods(
        entity.private_functions, "Private Functions", include_private))

    result.append("</nav>\n")
    consume result

  fun _render_toc_methods(
    methods: Array[DocMethod] val,
    title: String,
    include_private: Bool)
    : String val
  =>
    if methods.size() == 0 then return "" end

    let result = recover iso String(512) end
    result.append("<h3>")
    result.append(title)
    result.append("</h3>\n<ul>\n")

    for method in methods.values() do
      result.append("<li><a href=\"#")
      result.append(HtmlEscape.attribute(method.name))
      result.append("\"><code>")
      result.append(HtmlEscape.content(method.kind.string()))
      result.append(" ")
      result.append(HtmlEscape.content(method.name))
      result.append(HtmlEscape.content(
        TypeRenderer.render_type_params(
          method.type_params, None, false, include_private)))
      result.append(
        _toc_method_params(method, include_private))
      match method.kind
      | MethodConstructor | MethodFunction =>
        match method.return_type
        | let rt: DocType =>
          result.append(": ")
          result.append(HtmlEscape.content(
            TypeRenderer.render(rt, None, false, include_private)))
        end
        if method.is_partial then
          result.append(" ?")
        end
      end
      result.append("</code></a></li>\n")
    end

    result.append("</ul>\n")
    consume result

  fun _toc_method_params(
    method: DocMethod box,
    include_private: Bool)
    : String val
  =>
    let result = recover iso String(256) end
    result.append("(")
    for (i, param) in method.params.pairs() do
      if i > 0 then result.append(", ") end
      result.append(HtmlEscape.content(param.name))
      result.append(": ")
      result.append(HtmlEscape.content(
        TypeRenderer.render(
          param.param_type, None, false, include_private)))
    end
    result.append(")")
    consume result

  fun _render_toc_fields(
    fields: Array[DocField] val,
    title: String)
    : String val
  =>
    if fields.size() == 0 then return "" end

    let result = recover iso String(256) end
    result.append("<h3>")
    result.append(title)
    result.append("</h3>\n<ul>\n")

    for field in fields.values() do
      result.append("<li><a href=\"#")
      result.append(HtmlEscape.attribute(field.name))
      result.append("\"><code>")
      result.append(HtmlEscape.content(
        field.kind.string() + " " + field.name))
      result.append("</code></a></li>\n")
    end

    result.append("</ul>\n")
    consume result

  fun _render_provides_section(
    provides: Array[DocType] val,
    title: String,
    include_private: Bool)
    : String val
  =>
    if provides.size() == 0 then return "" end

    let result = recover iso String(256) end
    result.append("<h4>")
    result.append(title)
    result.append("</h4>\n<ul>\n")

    for p in provides.values() do
      result.append("<li>")
      result.append(
        TypeRenderer.render(p, HtmlLinkFormat, true, include_private))
      result.append("</li>\n")
    end

    result.append("</ul>\n<hr>\n")
    consume result

  fun _render_methods(
    methods: Array[DocMethod] val,
    title: String,
    sanitized_pkg: String,
    include_private: Bool)
    : String val
  =>
    if methods.size() == 0 then return "" end

    let result = recover iso String(2048) end
    result.append("<h2>")
    result.append(title)
    result.append("</h2>\n")

    for method in methods.values() do
      result.append(
        _render_method(method, sanitized_pkg, include_private))
    end

    consume result

  fun _render_method(
    method: DocMethod box,
    sanitized_pkg: String,
    include_private: Bool)
    : String val
  =>
    let result = recover iso String(1024) end

    result.append("<h3 id=\"")
    result.append(HtmlEscape.attribute(method.name))
    result.append("\">")
    result.append(HtmlEscape.content(method.name))
    result.append(HtmlEscape.content(
      TypeRenderer.render_type_params(
        method.type_params, None, false, include_private)))
    result.append(_source_link(method.source, sanitized_pkg))
    result.append("</h3>\n")

    match method.doc_string
    | let ds: String =>
      result.append("<p>")
      result.append(HtmlEscape.content(ds))
      result.append("</p>\n")
    end

    result.append("<pre><code>")
    result.append(HtmlEscape.content(method.kind.string()))
    result.append(" ")

    match method.kind
    | MethodConstructor | MethodFunction =>
      match method.cap
      | let c: String =>
        result.append(HtmlEscape.content(c))
        result.append(" ")
      end
    end

    result.append(HtmlEscape.content(method.name))
    result.append(HtmlEscape.content(
      TypeRenderer.render_type_params(
        method.type_params, None, true, include_private)))

    result.append("(")
    for (i, param) in method.params.pairs() do
      if i > 0 then
        result.append(",\n")
      else
        result.append("\n")
      end
      result.append("  ")
      result.append(HtmlEscape.content(param.name))
      result.append(": ")
      result.append(HtmlEscape.content(
        TypeRenderer.render(
          param.param_type, None, true, include_private)))
      match param.default_value
      | let dv: String =>
        result.append(" = ")
        result.append(HtmlEscape.content(dv))
      end
    end
    result.append(")")

    match method.kind
    | MethodConstructor | MethodFunction =>
      result.append("\n: ")
      match method.return_type
      | let rt: DocType =>
        result.append(HtmlEscape.content(
          TypeRenderer.render(rt, None, true, include_private)))
      end
      if method.is_partial then
        result.append(" ?")
      end
    end

    result.append("</code></pre>\n")

    if method.params.size() > 0 then
      result.append("<h4>Parameters</h4>\n<ul class=\"params\">\n")
      for param in method.params.values() do
        result.append("<li>")
        result.append(HtmlEscape.content(param.name))
        result.append(": ")
        result.append(
          TypeRenderer.render(
            param.param_type, HtmlLinkFormat, true, include_private))
        match param.default_value
        | let dv: String =>
          result.append(" = ")
          result.append(HtmlEscape.content(dv))
        end
        result.append("</li>\n")
      end
      result.append("</ul>\n")
    end

    match method.kind
    | MethodConstructor | MethodFunction =>
      result.append("<h4>Returns</h4>\n<ul class=\"returns\">\n<li>")
      match method.return_type
      | let rt: DocType =>
        result.append(
          TypeRenderer.render(
            rt, HtmlLinkFormat, true, include_private))
      end
      if method.is_partial then
        result.append(" ?")
      end
      result.append("</li>\n</ul>\n")
    end

    result.append("<hr>\n")
    consume result

  fun _render_fields(
    fields: Array[DocField] val,
    title: String,
    sanitized_pkg: String,
    include_private: Bool)
    : String val
  =>
    if fields.size() == 0 then return "" end

    let result = recover iso String(1024) end
    result.append("<h2>")
    result.append(title)
    result.append("</h2>\n")

    for field in fields.values() do
      result.append("<h3 id=\"")
      result.append(HtmlEscape.attribute(field.name))
      result.append("\">")
      result.append(HtmlEscape.content(
        field.kind.string() + " " + field.name))
      result.append(": ")
      result.append(
        TypeRenderer.render(
          field.field_type, HtmlLinkFormat, false, include_private))
      result.append(_source_link(field.source, sanitized_pkg))
      result.append("</h3>\n")

      match field.doc_string
      | let ds: String =>
        result.append("<p>")
        result.append(HtmlEscape.content(ds))
        result.append("</p>\n")
      end

      result.append("<hr>\n")
    end

    consume result

  fun _write_source_page(
    src_dir: FilePath,
    sanitized_pkg: String,
    sf: DocSourceFile box,
    site_name: String)
    ?
  =>
    let content = recover iso String(sf.content.size() + 2048) end
    let filename = sf.filename

    content.append("<h1>")
    content.append(HtmlEscape.content(filename))
    content.append("</h1>\n")
    content.append("<table class=\"source-table\">\n")

    var line_num: USize = 1
    var line_start: USize = 0
    var i: USize = 0
    let src = sf.content
    while i < src.size() do
      try
        if src(i)? == '\n' then
          let line: String val = src.substring(
            line_start.isize(), i.isize())
          content.append(_source_line_html(line_num, line))
          line_num = line_num + 1
          line_start = i + 1
        end
      end
      i = i + 1
    end
    // Last line (if no trailing newline)
    if line_start < src.size() then
      let line: String val = src.substring(
        line_start.isize(), src.size().isize())
      content.append(_source_line_html(line_num, line))
    end

    content.append("</table>\n")

    let root = "../../"
    let breadcrumb: String val =
      "<a href=\"" + root + "index.html\">Packages</a>"
    let page = _html_page(
      filename, site_name, breadcrumb, consume content
      where root_prefix = root)

    let pkg_dir = src_dir.join(sanitized_pkg)?
    let filename_no_ext = _remove_ext(filename)
    _write_file(pkg_dir, filename_no_ext + ".html", page)?

  fun _source_line_html(
    line_num: USize,
    line: String)
    : String val
  =>
    let result = recover iso String(line.size() + 128) end
    result.append("<tr>")
    result.append("<td class=\"line-num\" id=\"L")
    result.append(line_num.string())
    result.append("\"><a href=\"#L")
    result.append(line_num.string())
    result.append("\">")
    result.append(line_num.string())
    result.append("</a></td>")
    result.append("<td class=\"source-code\">")
    result.append(HtmlEscape.content(line))
    result.append("</td></tr>\n")
    consume result

  fun _write_assets(assets_dir: FilePath) ? =>
    _write_file(assets_dir, "style.css", _HtmlAssets.css())?
    _write_binary(assets_dir, "logo.png", _Assets.logo())?

  fun _source_link(
    source: (DocSourceLocation | None),
    sanitized_pkg: String)
    : String val
  =>
    match source
    | let loc: DocSourceLocation =>
      if loc.file_path.size() > 0 then
        let filename = Path.base(loc.file_path)
        let filename_no_ext = _remove_ext(filename)
        let result = recover iso String(128) end
        result.append(
          " <span class=\"source-link\">[<a href=\"src/")
        result.append(HtmlEscape.attribute(sanitized_pkg))
        result.append("/")
        result.append(HtmlEscape.attribute(filename_no_ext))
        result.append(".html#L")
        result.append(loc.line.string())
        result.append("\">Source</a>]</span>")
        consume result
      else
        ""
      end
    else
      ""
    end

  fun _remove_ext(filename: String): String =>
    var last_dot: USize = filename.size()
    var i: USize = 0
    while i < filename.size() do
      try if filename(i)? == '.' then last_dot = i end end
      i = i + 1
    end
    if last_dot < filename.size() then
      filename.substring(0, last_dot.isize())
    else
      filename
    end

  fun _write_file(
    dir: FilePath,
    name: String,
    content: String val)
    ?
  =>
    let path = dir.join(name)?
    match CreateFile(path)
    | let file: File =>
      file.write(content)
      file.dispose()
    else
      error
    end

  fun _write_binary(
    dir: FilePath,
    name: String,
    data: Array[U8] val)
    ?
  =>
    let path = dir.join(name)?
    match CreateFile(path)
    | let file: File =>
      file.write(data)
      file.dispose()
    else
      error
    end
