use "collections"
use "files"
use ast = "pony_compiler"

primitive Extractor
  """
  Walks a compiled `Program` and builds the documentation IR.

  Matches the traversal logic of `doc_packages()` and `doc_package()`
  in docgen.c (lines 1152-1256).
  """
  fun apply(program: ast.Program val, include_private: Bool): DocProgram =>
    // Build package map: AST pointer address → qualified name
    let package_map = _build_package_map(program)

    // Program name from root package directory basename
    let program_name =
      try
        let root_pkg = program.packages().next()?
        Path.base(root_pkg.path)
      else
        "unknown"
      end

    // Root package is always first, remaining sorted alphabetically
    let packages = recover val
      let result = Array[DocPackage]

      // Collect all packages
      let all_packages = Array[(ast.Package val, Bool)]
      var is_first = true
      for pkg in program.packages() do
        all_packages.push((pkg, is_first))
        is_first = false
      end

      // Process root package first (unconditionally)
      try
        (let root_pkg, _) = all_packages(0)?
        result.push(_extract_package(root_pkg, include_private,
          package_map))
      end

      // Sort remaining packages alphabetically by qualified name,
      // skipping test packages
      let remaining = Array[(ast.Package val, String)]
      for i in Range(1, all_packages.size()) do
        try
          (let pkg, _) = all_packages(i)?
          let qn = pkg.qualified_name
          if not Filter.is_test_package(qn) then
            remaining.push((pkg, qn))
          end
        end
      end

      // Sort by qualified name
      _sort_packages(remaining)

      for (pkg, _) in remaining.values() do
        result.push(_extract_package(pkg, include_private, package_map))
      end

      result
    end

    DocProgram(program_name, packages)

  fun _build_package_map(program: ast.Program val): Map[USize, String] val =>
    """
    Maps package AST pointer addresses to qualified names. Used by
    `_TypeExtractor` to resolve nominal type TQFNs across packages.
    """
    recover val
      let map = Map[USize, String]
      for pkg in program.packages() do
        map(pkg.ast.raw.usize()) = pkg.qualified_name
      end
      map
    end

  fun _sort_packages(
    packages: Array[(ast.Package val, String)])
  =>
    """Sort packages in-place by qualified name."""
    // Simple insertion sort — package count is small
    for i in Range(1, packages.size()) do
      try
        var j = i
        while (j > 0) and
          (packages(j)? ._2 < packages(j - 1)? ._2)
        do
          let tmp = packages(j)?
          packages(j)? = packages(j - 1)?
          packages(j - 1)? = tmp
          j = j - 1
        end
      end
    end

  fun _extract_package(
    pkg: ast.Package val,
    include_private: Bool,
    package_map: Map[USize, String] val)
    : DocPackage
  =>
    """Extracts a `DocPackage` from a compiled package."""
    var doc_string: (String | None) = None
    let public_entities = Array[DocEntity]
    let private_entities = Array[DocEntity]
    // Track which modules have documented entities for source file filtering
    let documented_modules = Map[String, ast.Module val]

    // Iterate TK_PACKAGE children
    for child in pkg.ast.children() do
      if child.id() == ast.TokenIds.tk_string() then
        // Package docstring is a direct TK_STRING child of TK_PACKAGE
        doc_string = child.token_value()
      elseif child.id() == ast.TokenIds.tk_module() then
        // Module — collect entities from its children, skipping TK_USE
        let module_for_child = _find_module_for_ast(pkg, child)

        for entity_ast in child.children() do
          if entity_ast.id() == ast.TokenIds.tk_use() then
            continue
          end

          // Filter: nodoc, internal names
          if Filter.has_nodoc(entity_ast) then continue end

          let entity_name =
            try
              (entity_ast(0)?.token_value() as String)
            else
              continue
            end

          if Filter.is_internal(entity_name) then continue end

          // Privacy filtering
          let is_priv = Filter.is_private(entity_name)
          if is_priv and (not include_private) then continue end

          // This entity is documented — record its module for source files
          match module_for_child
          | let m: ast.Module val =>
            documented_modules(m.file) = m
          end

          let entity = _extract_entity(entity_ast, include_private,
            package_map)

          if is_priv then
            private_entities.push(entity)
          else
            public_entities.push(entity)
          end
        end
      end
    end

    // Sort entities by name (ignoring leading underscore)
    let sorted_public = _sort_entities(public_entities)
    let sorted_private = _sort_entities(private_entities)

    // Collect source files from documented modules only
    let source_files = _collect_source_files(pkg.qualified_name,
      documented_modules)

    DocPackage(pkg.qualified_name, doc_string, sorted_public,
      sorted_private, source_files)

  fun _find_module_for_ast(
    pkg: ast.Package val,
    module_ast: ast.AST box)
    : (ast.Module val | None)
  =>
    """Find the Module wrapper for a given TK_MODULE AST node."""
    for m in pkg.modules() do
      if m.ast.raw.usize() == module_ast.raw.usize() then
        return m
      end
    end
    None

  fun _extract_entity(
    entity_ast: ast.AST box,
    include_private: Bool,
    package_map: Map[USize, String] val)
    : DocEntity
  =>
    """
    Extracts a `DocEntity` from an entity AST node.

    Child access (post-REORDER): [0] id, [1] type_params, [2] cap,
    [3] provides, [4] members, [5] c_api, [6] doc.
    """
    try
      let id_node = entity_ast(0)?
      let name = (id_node.token_value() as String)
      let kind = EntityKindBuilder(entity_ast.id())?

      // TQFN for this entity
      let tqfn = _resolve_entity_tqfn(entity_ast, package_map)

      // Type params
      let tparams_node = entity_ast(1)?
      let type_params = _extract_type_params(tparams_node, package_map)

      // Cap
      let cap_node = entity_ast(2)?
      let cap = _TypeExtractor.get_cap(cap_node)

      // Provides
      let provides_node = entity_ast(3)?
      let provides = _extract_provides(provides_node, package_map)

      // Doc string
      let doc_node = entity_ast(6)?
      let doc_string: (String | None) =
        if doc_node.id() != ast.TokenIds.tk_none() then
          doc_node.token_value()
        else
          None
        end

      // Source location
      let source = DocSourceLocation(
        try
          (entity_ast.source_file() as String)
        else
          ""
        end,
        entity_ast.line())

      // Members
      let members_node = entity_ast(4)?
      (let constructors, let public_fields, let public_behaviours,
        let public_functions, let private_behaviours,
        let private_functions) =
        _extract_members(members_node, include_private, package_map)

      DocEntity(kind, name, tqfn, type_params, cap, provides, doc_string,
        constructors, public_fields, public_behaviours, public_functions,
        private_behaviours, private_functions, source)
    else
      _empty_entity()
    end

  fun _resolve_entity_tqfn(
    entity_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : String
  =>
    """Resolves the TQFN for an entity by walking up to its TK_PACKAGE."""
    try
      let entity_name = (entity_ast(0)?.token_value() as String)
      var parent = entity_ast.parent()
      while true do
        match parent
        | let p: ast.AST =>
          if p.id() == ast.TokenIds.tk_package() then
            let pkg_name = package_map(p.raw.usize())?
            return TQFN(pkg_name, entity_name)
          end
          parent = p.parent()
        else
          break
        end
      end
      ""
    else
      ""
    end

  fun _extract_type_params(
    tparams_node: ast.AST box,
    package_map: Map[USize, String] val)
    : Array[DocTypeParam] val
  =>
    """
    Extracts type parameters.

    TypeParam child access: [0] id, [1] constraint, [2] default_type.
    """
    let result: Array[DocTypeParam] iso = recover iso Array[DocTypeParam] end
    for tp in tparams_node.children() do
      if tp.id() == ast.TokenIds.tk_none() then continue end
      try
        let tp_name = (tp(0)?.token_value() as String)
        let constraint: (DocType | None) =
          try
            let c = tp(1)?
            if c.id() != ast.TokenIds.tk_none() then
              _TypeExtractor(c, package_map)
            else
              None
            end
          else
            None
          end
        let default_type: (DocType | None) =
          try
            let d = tp(2)?
            if d.id() != ast.TokenIds.tk_none() then
              _TypeExtractor(d, package_map)
            else
              None
            end
          else
            None
          end
        result.push(DocTypeParam(tp_name, constraint, default_type))
      end
    end
    consume result

  fun _extract_provides(
    provides_node: ast.AST box,
    package_map: Map[USize, String] val)
    : Array[DocType] val
  =>
    """Extracts the provides/implements type list."""
    let result: Array[DocType] iso = recover iso Array[DocType] end
    if provides_node.id() != ast.TokenIds.tk_none() then
      for child in provides_node.children() do
        result.push(_TypeExtractor(child, package_map))
      end
      // If provides has no children but is itself a type, extract it
      if (result.size() == 0)
        and (provides_node.id() != ast.TokenIds.tk_provides())
      then
        result.push(_TypeExtractor(provides_node, package_map))
      end
    end
    consume result

  fun _extract_members(
    members_node: ast.AST box,
    include_private: Bool,
    package_map: Map[USize, String] val)
    : ( Array[DocMethod] val,  // constructors
        Array[DocField] val,   // public_fields
        Array[DocMethod] val,  // public_behaviours
        Array[DocMethod] val,  // public_functions
        Array[DocMethod] val,  // private_behaviours
        Array[DocMethod] val ) // private_functions
  =>
    """
    Sorts entity members into categories matching docgen.c lines 1035-1073.

    Fields are always public-only. Constructors are filtered by
    `include_private`. Behaviours and functions are split into
    public and private lists.
    """
    let constructors: Array[DocMethod] iso = recover iso Array[DocMethod] end
    let pub_fields: Array[DocField] iso = recover iso Array[DocField] end
    let pub_bes: Array[DocMethod] iso = recover iso Array[DocMethod] end
    let pub_funs: Array[DocMethod] iso = recover iso Array[DocMethod] end
    let priv_bes: Array[DocMethod] iso = recover iso Array[DocMethod] end
    let priv_funs: Array[DocMethod] iso = recover iso Array[DocMethod] end

    for member in members_node.children() do
      if Filter.has_nodoc(member) then continue end

      let member_name =
        try
          let name_idx: USize =
            match member.id()
            | ast.TokenIds.tk_fvar()
            | ast.TokenIds.tk_flet()
            | ast.TokenIds.tk_embed() => USize(0) // Field: [0] id
            | ast.TokenIds.tk_new()
            | ast.TokenIds.tk_fun()
            | ast.TokenIds.tk_be() => USize(1) // Method: [1] id
            else
              continue
            end
          (member(name_idx)?.token_value() as String)
        else
          continue
        end

      if Filter.is_internal(member_name) then continue end

      let is_priv = Filter.is_private(member_name)

      // Deliberate divergence from docgen.c: unrecognized member tokens are
      // silently skipped (no else branch) instead of asserting. docgen.c uses
      // pony_assert(0) in its default case, but graceful degradation is safer
      // here since pony-doc runs as a standalone tool rather than inside the
      // compiler pipeline.
      match member.id()
      | ast.TokenIds.tk_fvar()
      | ast.TokenIds.tk_flet()
      | ast.TokenIds.tk_embed() =>
        // Fields: public only (allow_public=true, allow_private=false)
        if not is_priv then
          pub_fields.push(_extract_field(member, package_map))
        end

      | ast.TokenIds.tk_new() =>
        // Constructors: public always, private if include_private
        if is_priv and (not include_private) then continue end
        constructors.push(_extract_method(member, package_map))

      | ast.TokenIds.tk_be() =>
        // Behaviours: public list gets public, private list gets private
        if is_priv then
          if include_private then
            priv_bes.push(_extract_method(member, package_map))
          end
        else
          pub_bes.push(_extract_method(member, package_map))
        end

      | ast.TokenIds.tk_fun() =>
        // Functions: public list gets public, private list gets private
        if is_priv then
          if include_private then
            priv_funs.push(_extract_method(member, package_map))
          end
        else
          pub_funs.push(_extract_method(member, package_map))
        end
      end
    end

    ( consume constructors,
      consume pub_fields,
      consume pub_bes,
      consume pub_funs,
      consume priv_bes,
      consume priv_funs )

  fun _extract_method(
    method_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : DocMethod
  =>
    """
    Extracts a `DocMethod` from a method AST node.

    Child access (post-REORDER): [0] cap, [1] id, [2] type_params,
    [3] params, [4] return_type, [5] error, [6] body, [7] doc.
    """
    try
      let kind = MethodKindBuilder(method_ast.id())?
      let name = (method_ast(1)?.token_value() as String)

      // Cap — only for fun and new (not be)
      let cap: (String | None) =
        if (method_ast.id() == ast.TokenIds.tk_fun())
          or (method_ast.id() == ast.TokenIds.tk_new())
        then
          _TypeExtractor.get_cap(method_ast(0)?)
        else
          None
        end

      let type_params = _extract_type_params(method_ast(2)?, package_map)

      // Params
      let params = _extract_params(method_ast(3)?, package_map)

      // Return type — only for fun and new
      let return_type: (DocType | None) =
        if (method_ast.id() == ast.TokenIds.tk_fun())
          or (method_ast.id() == ast.TokenIds.tk_new())
        then
          let ret_node = method_ast(4)?
          if ret_node.id() != ast.TokenIds.tk_none() then
            _TypeExtractor(ret_node, package_map)
          else
            None
          end
        else
          None
        end

      // Partial (?)
      let is_partial = (method_ast(5)?.id() == ast.TokenIds.tk_question())

      // Doc string
      let doc_node = method_ast(7)?
      let doc_string: (String | None) =
        if doc_node.id() != ast.TokenIds.tk_none() then
          doc_node.token_value()
        else
          None
        end

      // Source location
      let source = DocSourceLocation(
        try
          (method_ast.source_file() as String)
        else
          ""
        end,
        method_ast.line())

      DocMethod(kind, name, cap, type_params, params, return_type,
        is_partial, doc_string, source)
    else
      _empty_method()
    end

  fun _extract_field(
    field_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : DocField
  =>
    """
    Extracts a `DocField` from a field AST node.

    Child access (post-REORDER): [0] id, [1] type, [2] init, [3] doc.
    """
    try
      let kind = FieldKindBuilder(field_ast.id())?
      let name = (field_ast(0)?.token_value() as String)

      let field_type = _TypeExtractor(field_ast(1)?, package_map)

      let doc_node = field_ast(3)?
      let doc_string: (String | None) =
        if doc_node.id() != ast.TokenIds.tk_none() then
          doc_node.token_value()
        else
          None
        end

      let source = DocSourceLocation(
        try
          (field_ast.source_file() as String)
        else
          ""
        end,
        field_ast.line())

      DocField(kind, name, field_type, doc_string, source)
    else
      _empty_field()
    end

  fun _extract_params(
    params_node: ast.AST box,
    package_map: Map[USize, String] val)
    : Array[DocParam] val
  =>
    """
    Extracts method parameters.

    Param child access: [0] id, [1] type, [2] default.
    """
    let result: Array[DocParam] iso = recover iso Array[DocParam] end
    for param in params_node.children() do
      if param.id() == ast.TokenIds.tk_none() then continue end
      try
        let name = (param(0)?.token_value() as String)
        let param_type = _TypeExtractor(param(1)?, package_map)
        let default_value = _TypeExtractor.get_default_value(param(2)?)
        result.push(DocParam(name, param_type, default_value))
      end
    end
    consume result

  fun _sort_entities(
    entities: Array[DocEntity])
    : Array[DocEntity] val
  =>
    """Sort entities by name, ignoring leading underscore."""
    // Simple insertion sort in-place — entity count per package is small
    for i in Range(1, entities.size()) do
      try
        var j = i
        while (j > 0) and
          (NameSort.compare(entities(j)?.name, entities(j - 1)?.name)
            is Less)
        do
          let tmp = entities(j)?
          entities(j)? = entities(j - 1)?
          entities(j - 1)? = tmp
          j = j - 1
        end
      end
    end
    // Build a val array from the sorted ref array
    let sorted: Array[DocEntity] iso =
      recover iso Array[DocEntity](entities.size()) end
    for e in entities.values() do
      sorted.push(e)
    end
    consume sorted

  fun _collect_source_files(
    package_name: String,
    documented_modules: Map[String, ast.Module val])
    : Array[DocSourceFile] val
  =>
    """
    Collects source files from modules that contain documented entities.
    `source_contents()` returns `String box`, so content is cloned for
    `val` storage.
    """
    let result: Array[DocSourceFile] iso =
      recover iso Array[DocSourceFile] end
    for (_, m) in documented_modules.pairs() do
      let content =
        match m.ast.source_contents()
        | let s: String box => s.clone()
        else
          continue
        end
      let filename = Path.base(m.file)
      result.push(DocSourceFile(package_name, filename, consume content))
    end
    consume result

  fun _empty_entity(): DocEntity =>
    """Fallback for extraction errors."""
    DocEntity(EntityClass, "unknown", "",
      recover val Array[DocTypeParam] end, None,
      recover val Array[DocType] end, None,
      recover val Array[DocMethod] end,
      recover val Array[DocField] end,
      recover val Array[DocMethod] end,
      recover val Array[DocMethod] end,
      recover val Array[DocMethod] end,
      recover val Array[DocMethod] end,
      None)

  fun _empty_method(): DocMethod =>
    """Fallback for extraction errors."""
    DocMethod(MethodFunction, "unknown", None,
      recover val Array[DocTypeParam] end,
      recover val Array[DocParam] end,
      None, false, None, None)

  fun _empty_field(): DocField =>
    """Fallback for extraction errors."""
    DocField(FieldLet, "unknown",
      DocCapability("unknown"), None, None)
