use "files"

primitive MkDocsBackend is Backend
  """
  Generates MkDocs-compatible documentation output.

  Matches the output format of docgen.c in ponyc: mkdocs.yml configuration,
  package index pages, entity type pages, source code pages, and theme assets.
  """
  fun generate(
    program: DocProgram box,
    output_dir: FilePath,
    include_private: Bool)
    ?
  =>
    // Create directory structure: <name>-docs/docs/{src,assets}
    let base_name: String val = program.name + "-docs"
    let base_dir = output_dir.join(base_name)?
    base_dir.mkdir()
    let docs_dir = base_dir.join("docs")?
    docs_dir.mkdir()
    let src_dir = docs_dir.join("src")?
    src_dir.mkdir()
    let assets_dir = docs_dir.join("assets")?
    assets_dir.mkdir()

    // Accumulate content for mkdocs.yml nav and index.md
    let nav = recover iso String(4096) end
    let home_links = recover iso String(1024) end
    let source_entries = Array[String]

    // Track already-written source files to avoid duplicates
    let written_sources = Array[String]

    for pkg in program.packages.values() do
      let pkg_tqfn = TQFN(pkg.qualified_name, "-index")
      let sanitized_pkg =
        PathSanitize.replace_path_separator(pkg.qualified_name)

      // Nav: package group header and package index entry
      nav.append("- package ")
      nav.append(pkg.qualified_name)
      nav.append(":\n")
      nav.append("  - Package: \"")
      nav.append(pkg_tqfn)
      nav.append(".md\"\n")

      // Home page link
      home_links.append("* [")
      home_links.append(pkg.qualified_name)
      home_links.append("](")
      home_links.append(pkg_tqfn)
      home_links.append(".md)\n")

      // Build package page and entity nav entries
      let public_type_links = recover iso String(512) end
      let private_type_links = recover iso String(512) end

      for entity in pkg.public_types.values() do
        nav.append("  - ")
        nav.append(entity.kind.keyword())
        nav.append(" ")
        nav.append(entity.name)
        nav.append(": \"")
        nav.append(entity.tqfn)
        nav.append(".md\"\n")

        public_type_links.append("* [")
        public_type_links.append(entity.kind.keyword())
        public_type_links.append(" ")
        public_type_links.append(entity.name)
        public_type_links.append("](")
        public_type_links.append(entity.tqfn)
        public_type_links.append(".md)\n")

        _write_entity_page(docs_dir, entity, sanitized_pkg,
          include_private)?
      end

      for entity in pkg.private_types.values() do
        nav.append("  - ")
        nav.append(entity.kind.keyword())
        nav.append(" ")
        nav.append(entity.name)
        nav.append(": \"")
        nav.append(entity.tqfn)
        nav.append(".md\"\n")

        private_type_links.append("* [")
        private_type_links.append(entity.kind.keyword())
        private_type_links.append(" ")
        private_type_links.append(entity.name)
        private_type_links.append("](")
        private_type_links.append(entity.tqfn)
        private_type_links.append(".md)\n")

        _write_entity_page(docs_dir, entity, sanitized_pkg,
          include_private)?
      end

      // Write package index page
      _write_package_page(docs_dir, pkg, pkg_tqfn,
        consume public_type_links, consume private_type_links,
        include_private)?

      // Write source files
      for sf in pkg.source_files.values() do
        let source_key: String val = sanitized_pkg + "/" + sf.filename
        // Check for duplicates
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
          _write_source_page(src_dir, sanitized_pkg, sf)?

          source_entries.push(
            "  - " + sf.filename + " : \""
              + _source_doc_path(sanitized_pkg, sf.filename) + "\" \n")
        end
      end
    end

    // Sort source nav entries alphabetically and append
    if source_entries.size() > 0 then
      _sort_strings(source_entries)
      nav.append("- source:\n")
      for entry in source_entries.values() do
        nav.append(entry)
      end
    end

    // Write mkdocs.yml
    _write_mkdocs_yml(base_dir, program.name, consume nav)?

    // Write index.md (home page)
    _write_home_page(docs_dir, consume home_links)?

    // Write assets
    _write_assets(assets_dir)?

  fun _write_mkdocs_yml(
    base_dir: FilePath,
    site_name: String,
    nav_entries: String val)
    ?
  =>
    """Write the mkdocs.yml configuration file."""
    let content = recover iso String(2048) end
    content.append("site_name: ")
    content.append(site_name)
    content.append("\ntheme:\n")
    content.append("  name: material\n")
    content.append("  logo: assets/logo.png\n")
    content.append("  favicon: assets/logo.png\n")
    content.append("  palette:\n")
    content.append("    scheme: ponylang\n")
    content.append("    primary: brown\n")
    content.append("    accent: amber\n")
    content.append("  features:\n")
    content.append("    - navigation.top\n")
    content.append("extra_css:\n")
    content.append("  - assets/ponylang.css\n")
    content.append("markdown_extensions:\n")
    content.append("  - pymdownx.highlight:\n")
    content.append("      anchor_linenums: true\n")
    content.append("      line_anchors: \"L\"\n")
    content.append("      use_pygments: true\n")
    content.append("  - pymdownx.superfences\n")
    content.append("  - toc:\n")
    content.append("      permalink: true\n")
    content.append("      toc_depth: 3\n")
    content.append("nav:\n")
    content.append("- ")
    content.append(site_name)
    content.append(": index.md\n")
    content.append(nav_entries)
    _write_file(base_dir, "mkdocs.yml", consume content)?

  fun _write_home_page(docs_dir: FilePath, links: String val) ? =>
    """Write the index.md home page."""
    let content = recover iso String(512) end
    content.append("---\nsearch:\n  exclude: true\n---\n")
    content.append("Packages\n\n")
    content.append(links)
    _write_file(docs_dir, "index.md", consume content)?

  fun _write_package_page(
    docs_dir: FilePath,
    pkg: DocPackage box,
    pkg_tqfn: String,
    public_type_links: String val,
    private_type_links: String val,
    include_private: Bool)
    ?
  =>
    """
    Write a package index page.

    Matches `doc_package_home()` and the type listing append in
    `doc_package()` (lines 1096-1217 of docgen.c).
    """
    let content = recover iso String(1024) end

    // Package docstring or placeholder
    match pkg.doc_string
    | let ds: String =>
      content.append(ds)
    else
      content.append("No package doc string provided for ")
      content.append(pkg.qualified_name)
      content.append(".")
    end

    // Public types listing
    if public_type_links.size() > 0 then
      content.append("\n\n## Public Types\n\n")
      content.append(public_type_links)
    end

    // Private types listing
    if include_private and (private_type_links.size() > 0) then
      content.append("\n\n## Private Types\n\n")
      content.append(private_type_links)
    end

    let filename: String val = pkg_tqfn + ".md"
    _write_file(docs_dir, filename, consume content)?

  fun _write_entity_page(
    docs_dir: FilePath,
    entity: DocEntity box,
    sanitized_pkg: String,
    include_private: Bool)
    ?
  =>
    """
    Write an entity type page.

    Matches `doc_entity()` in docgen.c (lines 946-1092).
    """
    let content = recover iso String(4096) end

    // Heading: # Name[TypeParams]<source-link>
    content.append("# ")
    content.append(entity.name)
    content.append(
      TypeRenderer.render_type_params(entity.type_params, true, false,
        include_private))
    content.append(_source_link(entity.source, sanitized_pkg))

    // Docstring
    match entity.doc_string
    | let ds: String =>
      content.append("\n")
      content.append(ds)
      content.append("\n\n")
    end

    // Code block
    content.append("```pony\n")
    content.append(entity.kind.keyword())
    content.append(" ")
    match entity.cap
    | let c: String =>
      content.append(c)
      content.append(" ")
    end
    content.append(entity.name)
    content.append(
      TypeRenderer.render_type_params(entity.type_params, false, true,
        include_private))
    content.append(
      TypeRenderer.render_provides(entity.provides, " is\n  ", ",\n  ", "",
        false, include_private))
    content.append("\n```\n\n")

    // Implements / Type Alias For section
    match entity.kind
    | EntityTypeAlias =>
      content.append(
        TypeRenderer.render_provides(entity.provides,
          "#### Type Alias For\n\n* ", "\n* ", "\n\n---\n\n",
          true, include_private))
    else
      content.append(
        TypeRenderer.render_provides(entity.provides,
          "#### Implements\n\n* ", "\n* ", "\n\n---\n\n",
          true, include_private))
    end

    // Member sections
    content.append(
      _render_methods(entity.constructors, "Constructors",
        sanitized_pkg, include_private))
    content.append(
      _render_fields(entity.public_fields, "Public fields",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(entity.public_behaviours, "Public Behaviours",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(entity.public_functions, "Public Functions",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(entity.private_behaviours, "Private Behaviours",
        sanitized_pkg, include_private))
    content.append(
      _render_methods(entity.private_functions, "Private Functions",
        sanitized_pkg, include_private))

    let filename: String val = entity.tqfn + ".md"
    _write_file(docs_dir, filename, consume content)?

  fun _render_methods(
    methods: Array[DocMethod] val,
    title: String,
    sanitized_pkg: String,
    include_private: Bool)
    : String val
  =>
    """Render a methods section."""
    if methods.size() == 0 then return "" end

    let result = recover iso String(2048) end
    result.append("## ")
    result.append(title)
    result.append("\n\n")

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
    """
    Render a single method's documentation.

    Matches `doc_method()` in docgen.c (lines 693-762).
    """
    let result = recover iso String(1024) end

    // Heading: ### name[TypeParams]<source-link>
    result.append("### ")
    result.append(method.name)
    result.append(
      TypeRenderer.render_type_params(method.type_params, true, false,
        include_private))
    result.append(_source_link(method.source, sanitized_pkg))
    result.append("\n\n")

    // Docstring
    match method.doc_string
    | let ds: String =>
      result.append(ds)
      result.append("\n\n")
    end

    // Code block
    result.append("```pony\n")
    result.append(method.kind.string())
    result.append(" ")

    // Cap for fun and new only
    match method.kind
    | MethodConstructor | MethodFunction =>
      match method.cap
      | let c: String =>
        result.append(c)
        result.append(" ")
      end
    end

    result.append(method.name)
    result.append(
      TypeRenderer.render_type_params(method.type_params, false, true,
        include_private))

    // Parameters in code block
    result.append("(")
    for (i, param) in method.params.pairs() do
      if i > 0 then
        result.append(",\n")
      else
        result.append("\n")
      end
      result.append("  ")
      result.append(param.name)
      result.append(": ")
      result.append(
        TypeRenderer.render(param.param_type, false, true, include_private))
      match param.default_value
      | let dv: String =>
        result.append(" = ")
        result.append(dv)
      end
    end
    result.append(")")

    // Return type for fun and new
    match method.kind
    | MethodConstructor | MethodFunction =>
      result.append("\n: ")
      match method.return_type
      | let rt: DocType =>
        result.append(
          TypeRenderer.render(rt, false, true, include_private))
      end
      if method.is_partial then
        result.append(" ?")
      end
    end

    result.append("\n```\n")

    // Parameters list — matches docgen.c list_doc_params which always
    // emits a trailing "\n" even when there are zero parameters.
    if method.params.size() > 0 then
      result.append("#### Parameters\n\n")
      for param in method.params.values() do
        result.append("*   ")
        result.append(param.name)
        result.append(": ")
        result.append(
          TypeRenderer.render(param.param_type, true, true,
            include_private))
        match param.default_value
        | let dv: String =>
          result.append(" = ")
          result.append(dv)
        end
        result.append("\n")
      end
    end
    result.append("\n")

    // Returns section for fun and new
    match method.kind
    | MethodConstructor | MethodFunction =>
      result.append("#### Returns\n\n")
      result.append("* ")
      match method.return_type
      | let rt: DocType =>
        result.append(
          TypeRenderer.render(rt, true, true, include_private))
      end
      if method.is_partial then
        result.append(" ?")
      end
      result.append("\n\n")
    end

    // Horizontal rule
    result.append("---\n\n")

    consume result

  fun _render_fields(
    fields: Array[DocField] val,
    title: String,
    sanitized_pkg: String,
    include_private: Bool)
    : String val
  =>
    """
    Render a fields section.

    Matches `doc_fields()` in docgen.c (lines 512-554).
    """
    if fields.size() == 0 then return "" end

    let result = recover iso String(1024) end
    result.append("## ")
    result.append(title)
    result.append("\n\n")

    for field in fields.values() do
      // ### {ftype} {name}: {type}<source-link>
      result.append("### ")
      result.append(field.kind.string())
      result.append(" ")
      result.append(field.name)
      result.append(": ")
      result.append(
        TypeRenderer.render(field.field_type, true, false, include_private))
      result.append(_source_link(field.source, sanitized_pkg))
      result.append("\n")

      match field.doc_string
      | let ds: String =>
        result.append(ds)
        result.append("\n\n")
      end

      result.append("\n\n---\n\n")
    end

    consume result

  fun _write_source_page(
    src_dir: FilePath,
    sanitized_pkg: String,
    sf: DocSourceFile box)
    ?
  =>
    """
    Write a source code page.

    Matches `copy_source_to_doc_src()` in docgen.c (lines 795-888).
    """
    let content = recover iso String(sf.content.size() + 256) end
    content.append("---\nhide:\n  - toc\nsearch:\n  exclude: true\n---\n")
    content.append("```````pony linenums=\"1\"\n")
    content.append(sf.content)
    content.append("\n```````")

    let pkg_dir = src_dir.join(sanitized_pkg)?
    let filename_no_ext = _remove_ext(sf.filename)
    let out_name: String val = filename_no_ext + ".md"
    _write_file(pkg_dir, out_name, consume content)?

  fun _write_assets(assets_dir: FilePath) ? =>
    """Write CSS and logo PNG files."""
    _write_file(assets_dir, "ponylang.css", _Assets.css())?
    _write_binary(assets_dir, "logo.png", _Assets.logo())?

  fun _source_link(
    source: (DocSourceLocation | None),
    sanitized_pkg: String)
    : String val
  =>
    """
    Build a source code link span.

    Matches `add_source_code_link()` in docgen.c (lines 486-505).
    Returns `\n\n` when no source information is available.
    """
    match source
    | let loc: DocSourceLocation =>
      if loc.file_path.size() > 0 then
        let filename = Path.base(loc.file_path)
        let filename_no_ext = _remove_ext(filename)
        let result = recover iso String(128) end
        result.append("\n<span class=\"source-link\">[[Source]](src/")
        result.append(sanitized_pkg)
        result.append("/")
        result.append(filename_no_ext)
        result.append(".md#L-0-")
        result.append(loc.line.string())
        result.append(")</span>\n")
        consume result
      else
        "\n\n"
      end
    else
      "\n\n"
    end

  fun _sort_strings(entries: Array[String]) =>
    """Sort strings alphabetically using insertion sort."""
    var i: USize = 1
    while i < entries.size() do
      try
        var j = i
        while (j > 0) and
          (entries(j)?.compare(entries(j - 1)?) is Less)
        do
          let tmp = entries(j)?
          entries(j)? = entries(j - 1)?
          entries(j - 1)? = tmp
          j = j - 1
        end
      end
      i = i + 1
    end

  fun _source_doc_path(sanitized_pkg: String, filename: String): String val =>
    """Build the docs-relative path for a source file."""
    let result = recover iso String(64) end
    result.append("src/")
    result.append(sanitized_pkg)
    result.append("/")
    result.append(_remove_ext(filename))
    result.append(".md")
    consume result

  fun _remove_ext(filename: String): String =>
    """Remove the file extension from a filename."""
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
    """Write a text file, creating or truncating as needed."""
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
    """Write a binary file, creating or truncating as needed."""
    let path = dir.join(name)?
    match CreateFile(path)
    | let file: File =>
      file.write(data)
      file.dispose()
    else
      error
    end
