use "files"
use "json"
use "pony_compiler"

use @get_compiler_exe_directory[Bool](
  output_path: Pointer[U8] tag,
  argv0: Pointer[U8] tag)
use @ponyint_pool_alloc_size[Pointer[U8] val](size: USize)
use @ponyint_pool_free_size[None](size: USize, p: Pointer[U8] tag)

actor AgentServer
  """
  Stdin/stdout JSON query server for AI agents working with compiled
  Pony programs. Compiles a package on startup, then accepts one
  JSON query per line and writes one JSON response per line.
  """
  let _env: Env
  let _argv0: String val
  let _user_flags: Array[String val] val
  var _program: (Program val | None)
  var _session: (CompileSession | None)
  var _package_path: (FilePath | None)
  let _input_buffer: String ref
  var _last_errors: Array[Error] val
  var _listening: Bool

  new create(
    env: Env,
    argv0: String val,
    user_flags: Array[String val] val = recover val Array[String val] end)
  =>
    _env = env
    _argv0 = argv0
    _user_flags = user_flags
    _program = None
    _session = None
    _package_path = None
    _input_buffer = String
    _last_errors = recover val Array[Error] end
    _listening = false

  be compile(package: FilePath, id: JSONValue = "") =>
    """
    Compile the package and begin listening for queries on stdin.
    """
    _package_path = package
    _env.err.print("compiling: " + package.path)

    let installation_paths = _find_installation_paths(_argv0)
    let pony_path = _get_pony_path(_env)

    let search_paths: Array[String val] val =
      recover val
        let tmp = installation_paths.clone()
        for p in pony_path.values() do
          tmp.push(p)
        end
        consume tmp
      end

    let session =
      CompileSession(
        package,
        search_paths
        where
        user_flags = _user_flags,
        release = false,
        verbosity = VerbosityQuiet,
        limit = PassFinaliser)

    match \exhaustive\ session.program()
    | let prog: Program val =>
      _program = prog
      _session = session
      _last_errors = recover val Array[Error] end
      _env.err.print("compilation successful")
      _emit_response(
        JSONObject
          .update("id", id)
          .update("status", "ready")
          .update("message", "compilation successful"))
      if _listening then
        _env.exitcode(0)
      else
        _start_listening()
        _listening = true
      end
    | None =>
      let errs = session.errors()
      _last_errors = errs
      _env.err.print(
        "compilation failed with " + errs.size().string() + " error(s)")
      var err_arr = JSONArray
      for e in errs.values() do
        err_arr = err_arr.push(e.msg)
      end
      _emit_response(
        JSONObject
          .update("id", id)
          .update("status", "error")
          .update("errors", err_arr))
      session.dispose()
      if not _listening then
        _env.exitcode(1)
      end
    end

  fun ref _start_listening() =>
    _env.input(
      object iso is InputNotify
        let _server: AgentServer tag = this

        fun ref apply(data: Array[U8] iso) =>
          _server._receive_data(consume data)

        fun ref dispose() =>
          _server._input_closed()
      end,
      512)

  be _receive_data(data: Array[U8] iso) =>
    _input_buffer.append(consume data)
    while true do
      try
        let nl = _input_buffer.find("\n")?
        let line = _input_buffer.substring(0, nl.isize())
        _input_buffer.delete(0, nl.usize() + 1)
        let trimmed = line.clone() .> strip()
        if trimmed.size() > 0 then
          _handle_line(consume trimmed)
        end
      else
        break
      end
    end

  be _input_closed() =>
    match _session
    | let s: CompileSession =>
      s.dispose()
    end

  fun ref _handle_line(line: String val) =>
    match \exhaustive\ JSONParser.parse(line)
    | let request: JSONValue =>
      let nav = JSONNav(request)
      let id_nav = nav("id")
      let id: JSONValue =
        if id_nav.found() then
          try
            id_nav.as_i64()?
          else
            try id_nav.as_string()?
            else "" end
          end
        else
          ""
        end

      let query_nav = nav("query")
      if query_nav.found() then
        try
          let query = query_nav.as_string()?
          match query
          | "inspect" =>
            _handle_inspect(id, nav)
          | "scope" =>
            _handle_scope(id, nav)
          | "exports" =>
            _handle_exports(id, nav)
          | "type_api" =>
            _handle_type_api(id, nav)
          | "check" =>
            _handle_check(id, nav)
          | "implementors" =>
            _handle_implementors(id, nav)
          | "errors" =>
            _handle_errors(id)
          | "reload" =>
            _handle_reload(id)
          | "status" =>
            _handle_status(id)
          else
            _emit_error(id, "unknown query: " + query)
          end
        else
          _emit_error(id, "query field must be a string")
        end
      else
        _emit_error(id, "missing 'query' field")
      end
    | let err: JSONParseError =>
      _emit_error("", "JSON parse error: " + err.message)
    end

  fun ref _handle_inspect(id: JSONValue, nav: JSONNav) =>
    try
      let file = nav("file").as_string()?
      let line = nav("line").as_i64()?
      let column = nav("column").as_i64()?

      match \exhaustive\ _session
      | let session: CompileSession =>
        let inspect_result =
          InspectQuery(session, file, line.usize(), column.usize())
        match \exhaustive\ inspect_result
        | let result: InspectResult val =>
          var def_obj = JSONObject
          match result.definition_file
          | let f: String val =>
            def_obj = def_obj.update("file", f)
          end
          if result.definition_line > 0 then
            def_obj = def_obj
              .update("line", I64.from[USize](result.definition_line))
              .update("column", I64.from[USize](result.definition_column))
          end
          match result.definition_kind
          | let k: String val =>
            def_obj = def_obj.update("kind", k)
          end
          match result.declared_type
          | let dt: String val =>
            def_obj = def_obj.update("declared_type", dt)
          end

          var enc_obj = JSONObject
          match result.enclosing_method
          | let m: String val =>
            enc_obj = enc_obj.update("method", m)
          end
          match result.enclosing_receiver_cap
          | let c: String val =>
            enc_obj = enc_obj.update("receiver_cap", c)
          end
          match result.enclosing_type
          | let t: String val =>
            enc_obj = enc_obj.update("type", t)
          end

          _emit_response(
            JSONObject
              .update("id", id)
              .update("type", result.type_string)
              .update("node_kind", result.node_kind)
              .update("line", I64.from[USize](result.line))
              .update("column", I64.from[USize](result.column))
              .update("definition", def_obj)
              .update("enclosing", enc_obj)
              .update("in_recover", result.in_recover)
              .update("sendable", result.sendable))
        | let err: String val =>
          _emit_error(id, err)
        end
      else
        _emit_error(id, "no compiled program available")
      end
    else
      _emit_error(id, "inspect requires 'file', 'line', 'column'")
    end

  fun ref _handle_scope(id: JSONValue, nav: JSONNav) =>
    try
      let file = nav("file").as_string()?
      let line = nav("line").as_i64()?
      let column = nav("column").as_i64()?

      match \exhaustive\ _session
      | let session: CompileSession =>
        let scope_result =
          ScopeQuery(session, file, line.usize(), column.usize())
        match \exhaustive\ scope_result
        | let result: ScopeResult val =>
          var entries_arr = JSONArray
          for entry in result.entries.values() do
            var entry_obj = JSONObject
              .update("name", entry.name)
              .update("type", entry.type_string)
              .update("kind", entry.kind)
              .update("accessible", entry.accessible)
            match entry.reason
            | let r: String val =>
              entry_obj = entry_obj.update("reason", r)
            end
            entries_arr = entries_arr.push(entry_obj)
          end
          _emit_response(
            JSONObject
              .update("id", id)
              .update("in_recover", result.in_recover)
              .update("entries", entries_arr))
        | let err: String val =>
          _emit_error(id, err)
        end
      else
        _emit_error(id, "no compiled program available")
      end
    else
      _emit_error(id, "scope requires 'file', 'line', 'column'")
    end

  fun ref _handle_exports(id: JSONValue, nav: JSONNav) =>
    try
      let package_name = nav("package").as_string()?

      match \exhaustive\ _session
      | let session: CompileSession =>
        let exports_result =
          ExportsQuery(session, package_name)
        match \exhaustive\ exports_result
        | let result: ExportsResult val =>
          var entries_arr = JSONArray
          for entry in result.entries.values() do
            var entry_obj = JSONObject
              .update("name", entry.name)
              .update("kind", entry.kind)
            match entry.docstring
            | let d: String val =>
              entry_obj = entry_obj.update("docstring", d)
            end
            entries_arr = entries_arr.push(entry_obj)
          end
          _emit_response(
            JSONObject
              .update("id", id)
              .update("package_path", result.package_path)
              .update("entries", entries_arr))
        | let err: String val =>
          _emit_error(id, err)
        end
      else
        _emit_error(id, "no compiled program available")
      end
    else
      _emit_error(id, "exports requires 'package'")
    end

  fun ref _handle_type_api(id: JSONValue, nav: JSONNav) =>
    match \exhaustive\ _session
    | let session: CompileSession =>
      let type_nav = nav("type")
      let cap_nav = nav("cap")
      let file_nav = nav("file")

      let api_result =
        if type_nav.found() and cap_nav.found() then
          try
            TypeAPIQuery.from_type(
              session,
              type_nav.as_string()?,
              cap_nav.as_string()?)
          else
            _emit_error(id, "type_api 'type' and 'cap' must be strings")
            return
          end
        elseif file_nav.found() then
          try
            TypeAPIQuery.from_position(
              session,
              file_nav.as_string()?,
              nav("line").as_i64()?.usize(),
              nav("column").as_i64()?.usize())
          else
            _emit_error(
              id,
              "type_api position requires 'file', 'line', 'column'")
            return
          end
        else
          _emit_error(
            id,
            "type_api requires 'type'+'cap' or 'file'+'line'+'column'")
          return
        end

      match \exhaustive\ api_result
      | let result: TypeAPIResult val =>
        var callable_arr = JSONArray
        for m in result.callable.values() do
          callable_arr = callable_arr.push(_method_to_json(m))
        end

        var not_callable_arr = JSONArray
        for (m, reason) in result.not_callable.values() do
          not_callable_arr =
            not_callable_arr.push(
              JSONObject
                .update("method", _method_to_json(m))
                .update("reason", reason))
        end

        var ctor_arr = JSONArray
        for m in result.constructors.values() do
          ctor_arr = ctor_arr.push(_method_to_json(m))
        end

        var traits_arr = JSONArray
        for t in result.traits.values() do
          traits_arr = traits_arr.push(t)
        end

        _emit_response(
          JSONObject
            .update("id", id)
            .update("type", result.type_name)
            .update("kind", result.type_kind)
            .update("callable", callable_arr)
            .update("not_callable", not_callable_arr)
            .update("constructors", ctor_arr)
            .update("traits", traits_arr))
      | let err: String val =>
        _emit_error(id, err)
      end
    else
      _emit_error(id, "no compiled program available")
    end

  fun _method_to_json(m: MethodInfo val): JSONObject =>
    var params_arr = JSONArray
    for p in m.params.values() do
      var param_obj = JSONObject
        .update("name", p.name)
        .update("type", p.param_type)
      if p.has_default then
        param_obj = param_obj.update("has_default", true)
      end
      params_arr = params_arr.push(param_obj)
    end

    JSONObject
      .update("name", m.name)
      .update("receiver", m.receiver_cap)
      .update("params", params_arr)
      .update("return", m.return_type)
      .update("partial", m.is_partial)
      .update("kind", m.kind)
      .update("source_type", m.source_type)

  fun ref _handle_check(id: JSONValue, nav: JSONNav) =>
    try
      let source = nav("source").as_string()?
      let target = nav("target").as_string()?

      match \exhaustive\ _session
      | let session: CompileSession =>
        let check_result = CheckQuery(session, source, target)
        match \exhaustive\ check_result
        | let result: CheckResult val =>
          var suggestions_arr = JSONArray
          for s in result.suggestions.values() do
            suggestions_arr = suggestions_arr.push(s)
          end
          _emit_response(
            JSONObject
              .update("id", id)
              .update("compatible", result.compatible)
              .update("source_type", result.source_type)
              .update("target_type", result.target_type)
              .update("reason", result.reason)
              .update("suggestions", suggestions_arr))
        | let err: String val =>
          _emit_error(id, err)
        end
      else
        _emit_error(id, "no compiled program available")
      end
    else
      _emit_error(id, "check requires 'source' and 'target'")
    end

  fun ref _handle_implementors(id: JSONValue, nav: JSONNav) =>
    try
      let type_name = nav("type").as_string()?

      match \exhaustive\ _session
      | let session: CompileSession =>
        let impl_result = ImplementorsQuery(session, type_name)
        match \exhaustive\ impl_result
        | let result: ImplementorsResult val =>
          var nominal_arr = JSONArray
          for n in result.nominal.values() do
            nominal_arr = nominal_arr.push(n)
          end
          var structural_arr = JSONArray
          for s in result.structural.values() do
            structural_arr = structural_arr.push(s)
          end
          _emit_response(
            JSONObject
              .update("id", id)
              .update("type", result.type_name)
              .update("nominal", nominal_arr)
              .update("structural", structural_arr))
        | let err: String val =>
          _emit_error(id, err)
        end
      else
        _emit_error(id, "no compiled program available")
      end
    else
      _emit_error(id, "implementors requires 'type'")
    end

  fun ref _handle_errors(id: JSONValue) =>
    var err_arr = JSONArray
    for e in _last_errors.values() do
      var err_obj = JSONObject.update("message", e.msg)
      match e.file
      | let f: String val =>
        err_obj = err_obj.update("file", f)
      end
      if e.position > Position.min() then
        err_obj = err_obj
          .update("line", I64.from[USize](e.position.line()))
          .update("column", I64.from[USize](e.position.column()))
      end
      err_arr = err_arr.push(err_obj)
    end
    _emit_response(
      JSONObject
        .update("id", id)
        .update("errors", err_arr))

  fun ref _handle_reload(id: JSONValue) =>
    match _package_path
    | let pkg: FilePath =>
      match _session
      | let old_session: CompileSession =>
        old_session.dispose()
      end
      _session = None
      _program = None
      compile(pkg, id)
    else
      _emit_error(id, "no package to reload")
    end

  fun ref _handle_status(id: JSONValue) =>
    match _program
    | let _: Program val =>
      _emit_response(
        JSONObject
          .update("id", id)
          .update("status", "ready"))
    else
      _emit_response(
        JSONObject
          .update("id", id)
          .update("status", "not_compiled"))
    end

  fun _emit_response(obj: JSONObject) =>
    _env.out.print(obj.print())

  fun _emit_error(id: JSONValue, message: String) =>
    _env.out.print(
      JSONObject
        .update("id", id)
        .update("error", message)
        .print())

  fun tag _find_installation_paths(argv0: String val): Array[String val] val =>
    match \exhaustive\ _find_exe_directory(argv0)
    | let dir: String val =>
      recover val
        Array[String val](2)
          .> push(recover val Path.join(dir, "../packages") end)
          .> push(recover val Path.join(dir, "../../packages") end)
      end
    | None =>
      recover val Array[String val] end
    end

  fun tag _find_exe_directory(argv0: String val): (String val | None) =>
    let buf_size: USize = 4096
    let buf = @ponyint_pool_alloc_size(buf_size)
    if @get_compiler_exe_directory(buf, argv0.cstring()) then
      let result = recover val String.copy_cstring(buf) end
      @ponyint_pool_free_size(buf_size, buf)
      result
    else
      @ponyint_pool_free_size(buf_size, buf)
      None
    end

  fun tag _get_pony_path(env: Env): Array[String val] val =>
    match \exhaustive\ PonyPath(env)
    | let p: String =>
      let split: Array[String] iso = Path.split_list(p)
      consume split
    | None =>
      recover val Array[String val] end
    end
