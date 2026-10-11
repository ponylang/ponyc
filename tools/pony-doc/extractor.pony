use "collections"
use "files"
use ast = "pony_compiler"

primitive Extractor
  """
  Walks a compiled `Program` and builds the documentation IR.
  """
  fun apply(
    program: ast.Program val,
    include_private: Bool)
    : DocProgram
  =>
    """
    Extract a documentation IR from a compiled program.
    """
    // Build module map: AST pointer address → qualified name
    let module_map = _build_module_map(program)

    // Program name from root module directory basename
    let program_name =
      try
        let root_mod = program.modules().next()?
        Path.base(root_mod.path)
      else
        "unknown"
      end

    // Root module is always first, remaining sorted alphabetically
    let modules =
      recover val
      let result = Array[DocModule]

      // Collect all modules
      let all_modules = Array[(ast.Module val, Bool)]
      var is_first = true
      for mod in program.modules() do
        all_modules.push((mod, is_first))
        is_first = false
      end

      // Process root module first (unconditionally)
      try
        (let root_mod, _) = all_modules(0)?
        result.push(
          _extract_module(
            root_mod,
            include_private,
            module_map))
      end

      // Sort remaining modules alphabetically by qualified name,
      // skipping test modules
      let remaining = Array[(ast.Module val, String)]
      for i in Range(1, all_modules.size()) do
        try
          (let mod, _) = all_modules(i)?
          let qn = mod.qualified_name
          if not Filter.is_test_module(qn) then
            remaining.push((mod, qn))
          end
        end
      end

      // Sort by qualified name
      _sort_modules(remaining)

      for (mod, _) in remaining.values() do
        result.push(_extract_module(mod, include_private, module_map))
      end

      result
    end

    DocProgram(program_name, modules)

  fun _build_module_map(program: ast.Program val): Map[USize, String] val =>
    """
    Maps module AST pointer addresses to qualified names. Used by
    `_TypeExtractor` to resolve nominal type TQFNs across modules.
    """
    recover val
      let map = Map[USize, String]
      for mod in program.modules() do
        map(mod.ast.raw.usize()) = mod.qualified_name
      end
      map
    end

  fun _sort_modules(
    modules: Array[(ast.Module val, String)])
  =>
    """
    Sort modules in-place by qualified name.
    """
    // Simple insertion sort — module count is small
    for i in Range(1, modules.size()) do
      try
        var j = i
        while (j > 0) and
          (modules(j)?._2 < modules(j - 1)?._2)
        do
          let tmp = modules(j)?
          modules(j)? = modules(j - 1)?
          modules(j - 1)? = tmp
          j = j - 1
        end
      end
    end

  fun _extract_module(
    mod: ast.Module val,
    include_private: Bool,
    module_map: Map[USize, String] val)
    : DocModule
  =>
    """
    Extracts a `DocModule` from a compiled module.
    """
    var doc_string: (String | None) = None
    let public_entities = Array[DocEntity]
    let private_entities = Array[DocEntity]
    // Track which files have documented entities for source file filtering
    let documented_files = Map[String, ast.SourceFile val]

    // Iterate TK_MODULE children
    for child in mod.ast.children() do
      if child.id() == ast.TokenIds.tk_string() then
        // Module docstring is a direct TK_STRING child of TK_MODULE
        doc_string = child.token_value()
      elseif child.id() == ast.TokenIds.tk_file() then
        // File — collect entities from its children, skipping TK_USE
        let file_for_child = _find_file_for_ast(mod, child)

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

          // This entity is documented — record its file for source files
          match file_for_child
          | let f: ast.SourceFile val =>
            documented_files(f.file) = f
          end

          let entity =
            _extract_entity(
              entity_ast,
              include_private,
              module_map)

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

    // Collect source files from documented files only
    let source_files =
      _collect_source_files(
        mod.qualified_name,
        documented_files)

    DocModule(
      mod.qualified_name,
      doc_string,
      sorted_public,
      sorted_private,
      source_files)

  fun _find_file_for_ast(
    mod: ast.Module val,
    file_ast: ast.AST box)
    : (ast.SourceFile val | None)
  =>
    """
    Find the File wrapper for a given TK_FILE AST node.
    """
    for f in mod.files() do
      if f.ast.raw.usize() == file_ast.raw.usize() then
        return f
      end
    end
    None

  fun _extract_entity(
    entity_ast: ast.AST box,
    include_private: Bool,
    module_map: Map[USize, String] val)
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
      let tqfn = _resolve_entity_tqfn(entity_ast, module_map)

      // Type params
      let tparams_node = entity_ast(1)?
      let type_params = _extract_type_params(tparams_node, module_map)

      // Cap
      let cap_node = entity_ast(2)?
      let cap = _TypeExtractor.get_cap(cap_node)

      // Provides
      let provides_node = entity_ast(3)?
      let provides = _extract_provides(provides_node, module_map)

      // Doc string
      let doc_node = entity_ast(6)?
      let doc_string: (String | None) =
        if doc_node.id() != ast.TokenIds.tk_none() then
          doc_node.token_value()
        else
          None
        end

      // Source location
      let source =
        DocSourceLocation(
          try
            (entity_ast.source_file() as String)
          else
            ""
          end,
          entity_ast.line())

      // Members
      let members_node = entity_ast(4)?
      (let constructors, let public_fields,
        let public_behaviours,
        let public_functions,
        let private_behaviours,
        let private_functions) =
        _extract_members(
          members_node,
          include_private,
          module_map)

      DocEntity(
        kind,
        name,
        tqfn,
        type_params,
        cap,
        provides,
        doc_string,
        constructors,
        public_fields,
        public_behaviours,
        public_functions,
        private_behaviours,
        private_functions,
        source)
    else
      _empty_entity()
    end

  fun _resolve_entity_tqfn(
    entity_ast: ast.AST box,
    module_map: Map[USize, String] val)
    : String
  =>
    """
    Resolves the TQFN for an entity by walking up to
    its TK_MODULE.
    """
    try
      let entity_name = (entity_ast(0)?.token_value() as String)
      var parent = entity_ast.parent()
      while true do
        match parent
        | let p: ast.AST =>
          if p.id() == ast.TokenIds.tk_module() then
            let mod_name = module_map(p.raw.usize())?
            return TQFN(mod_name, entity_name)
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
    module_map: Map[USize, String] val)
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
              _TypeExtractor(c, module_map)
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
              _TypeExtractor(d, module_map)
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
    module_map: Map[USize, String] val)
    : Array[DocType] val
  =>
    """
    Extracts the provides/implements type list.
    """
    let result: Array[DocType] iso = recover iso Array[DocType] end
    if provides_node.id() != ast.TokenIds.tk_none() then
      for child in provides_node.children() do
        result.push(_TypeExtractor(child, module_map))
      end
      // If provides has no children but is itself a type, extract it
      if (result.size() == 0) and
        (provides_node.id() != ast.TokenIds.tk_provides())
      then
        result.push(_TypeExtractor(provides_node, module_map))
      end
    end
    consume result

  fun _extract_members(
    members_node: ast.AST box,
    include_private: Bool,
    module_map: Map[USize, String] val)
    : ( Array[DocMethod] val,  // constructors
        Array[DocField] val,   // public_fields
        Array[DocMethod] val,  // public_behaviours
        Array[DocMethod] val,  // public_functions
        Array[DocMethod] val,  // private_behaviours
        Array[DocMethod] val ) // private_functions
  =>
    """
    Sorts entity members into categories.

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
            | ast.TokenIds.tk_methodgroup() =>
              for grouped in member.children() do
                if Filter.has_nodoc(grouped) then continue end
                let gname =
                  try
                    (grouped(1)?.token_value() as String)
                  else
                    continue
                  end
                if Filter.is_internal(gname) then continue end
                let gpriv = Filter.is_private(gname)
                match grouped.id()
                | ast.TokenIds.tk_new() =>
                  if gpriv and (not include_private) then continue end
                  constructors.push(_extract_method(grouped, module_map))
                | ast.TokenIds.tk_be() =>
                  if gpriv then
                    if include_private then
                      priv_bes.push(_extract_method(grouped, module_map))
                    end
                  else
                    pub_bes.push(_extract_method(grouped, module_map))
                  end
                | ast.TokenIds.tk_fun() =>
                  if gpriv then
                    if include_private then
                      priv_funs.push(_extract_method(grouped, module_map))
                    end
                  else
                    pub_funs.push(_extract_method(grouped, module_map))
                  end
                end
              end
              continue
            else
              continue
            end
          (member(name_idx)?.token_value() as String)
        else
          continue
        end

      if Filter.is_internal(member_name) then continue end

      let is_priv = Filter.is_private(member_name)

      // Unrecognized member tokens are silently skipped (no else branch)
      // rather than asserting, since pony-doc runs as a standalone tool.
      match member.id()
      | ast.TokenIds.tk_fvar()
      | ast.TokenIds.tk_flet()
      | ast.TokenIds.tk_embed() =>
        // Fields: public only (allow_public=true, allow_private=false)
        if not is_priv then
          pub_fields.push(_extract_field(member, module_map))
        end

      | ast.TokenIds.tk_new() =>
        // Constructors: public always, private if include_private
        if is_priv and (not include_private) then continue end
        constructors.push(_extract_method(member, module_map))

      | ast.TokenIds.tk_be() =>
        // Behaviours: public list gets public, private list gets private
        if is_priv then
          if include_private then
            priv_bes.push(_extract_method(member, module_map))
          end
        else
          pub_bes.push(_extract_method(member, module_map))
        end

      | ast.TokenIds.tk_fun() =>
        // Functions: public list gets public, private list gets private
        if is_priv then
          if include_private then
            priv_funs.push(_extract_method(member, module_map))
          end
        else
          pub_funs.push(_extract_method(member, module_map))
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
    module_map: Map[USize, String] val)
    : DocMethod
  =>
    """
    Extracts a `DocMethod` from a method AST node.

    Child access (post-REORDER): [0] cap, [1] id, [2] type_params,
    [3] params, [4] return_type, [5] guard, [6] error, [7] body, [8] doc.
    """
    try
      let kind = MethodKindBuilder(method_ast.id())?
      let name = (method_ast(1)?.token_value() as String)

      // Cap — only for fun and new (not be)
      let cap: (String | None) =
        if (method_ast.id() == ast.TokenIds.tk_fun()) or
          (method_ast.id() == ast.TokenIds.tk_new())
        then
          _TypeExtractor.get_cap(method_ast(0)?)
        else
          None
        end

      let type_params = _extract_type_params(method_ast(2)?, module_map)

      // Params
      let params = _extract_params(method_ast(3)?, module_map)

      // Return type — only for fun and new
      let return_type: (DocType | None) =
        if (method_ast.id() == ast.TokenIds.tk_fun()) or
          (method_ast.id() == ast.TokenIds.tk_new())
        then
          let ret_node = method_ast(4)?
          if ret_node.id() != ast.TokenIds.tk_none() then
            _TypeExtractor(ret_node, module_map)
          else
            None
          end
        else
          None
        end

      // Partial (?)
      let is_partial = (method_ast(6)?.id() == ast.TokenIds.tk_question())

      // Doc string
      let doc_node = method_ast(8)?
      let doc_string: (String | None) =
        if doc_node.id() != ast.TokenIds.tk_none() then
          doc_node.token_value()
        else
          None
        end

      // Source location
      let source =
        DocSourceLocation(
          try
            (method_ast.source_file() as String)
          else
            ""
          end,
          method_ast.line())

      DocMethod(
        kind,
        name,
        cap,
        type_params,
        params,
        return_type,
        is_partial,
        doc_string,
        source)
    else
      _empty_method()
    end

  fun _extract_field(
    field_ast: ast.AST box,
    module_map: Map[USize, String] val)
    : DocField
  =>
    """
    Extracts a `DocField` from a field AST node.

    Child access (post-REORDER): [0] id, [1] type, [2] init, [3] doc.
    """
    try
      let kind = FieldKindBuilder(field_ast.id())?
      let name = (field_ast(0)?.token_value() as String)

      let field_type = _TypeExtractor(field_ast(1)?, module_map)

      let doc_node = field_ast(3)?
      let doc_string: (String | None) =
        if doc_node.id() != ast.TokenIds.tk_none() then
          doc_node.token_value()
        else
          None
        end

      let source =
        DocSourceLocation(
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
    module_map: Map[USize, String] val)
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
        let param_type = _TypeExtractor(param(1)?, module_map)
        let default_value = _TypeExtractor.get_default_value(param(2)?)
        result.push(DocParam(name, param_type, default_value))
      end
    end
    consume result

  fun _sort_entities(
    entities: Array[DocEntity])
    : Array[DocEntity] val
  =>
    """
    Sort entities by name, ignoring leading underscore.
    """
    // Simple insertion sort in-place — entity count per package is small
    for i in Range(1, entities.size()) do
      try
        var j = i
        while (j > 0) and
          (NameSort.compare(
            entities(j)?.name, entities(j - 1)?.name) is Less)
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
    module_name: String,
    documented_files: Map[String, ast.SourceFile val])
    : Array[DocSourceFile] val
  =>
    """
    Collects source files from files that contain documented entities.
    `source_contents()` returns `String box`, so content is cloned for
    `val` storage.
    """
    let result: Array[DocSourceFile] iso =
      recover iso Array[DocSourceFile] end
    for (_, f) in documented_files.pairs() do
      let content =
        match f.ast.source_contents()
        | let s: String box => s.clone()
        else
          continue
        end
      let filename = Path.base(f.file)
      result.push(DocSourceFile(module_name, filename, consume content))
    end
    consume result

  fun _empty_entity(): DocEntity =>
    """
    Fallback for extraction errors.
    """
    DocEntity(
      EntityClass,
      "unknown",
      "",
      recover val Array[DocTypeParam] end,
      None,
      recover val Array[DocType] end,
      None,
      recover val Array[DocMethod] end,
      recover val Array[DocField] end,
      recover val Array[DocMethod] end,
      recover val Array[DocMethod] end,
      recover val Array[DocMethod] end,
      recover val Array[DocMethod] end,
      None)

  fun _empty_method(): DocMethod =>
    """
    Fallback for extraction errors.
    """
    DocMethod(
      MethodFunction,
      "unknown",
      None,
      recover val Array[DocTypeParam] end,
      recover val Array[DocParam] end,
      None,
      false,
      None,
      None)

  fun _empty_field(): DocField =>
    """
    Fallback for extraction errors.
    """
    DocField(
      FieldLet,
      "unknown",
      DocCapability("unknown"),
      None,
      None)
