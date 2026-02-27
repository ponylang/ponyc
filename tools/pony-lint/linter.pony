use "collections"
use "files"
use ast = "pony_compiler"

class val Linter
  """
  Orchestrates the lint pipeline: discover files, run text rules and AST rules,
  apply suppressions, and produce sorted diagnostics.

  The pipeline has two phases:

  1. **Text phase** — for each discovered .pony file, load the source, parse
     suppressions, and run enabled text rules with suppression filtering.
  2. **AST phase** — group files by directory (package), compile each package
     at PassParse via pony_compiler, then dispatch enabled AST rules against the
     parsed syntax tree. Module-level and package-level rules (file-naming,
     package-naming) run after the per-node dispatch.

  File discovery recursively finds .pony files in given targets, skipping
  _corral/, _repos/, and directories starting with a dot. When inside a git
  repository, `.gitignore` patterns are also respected; `.ignore` files are
  always respected regardless of git context. Hardcoded skips (`_corral/`,
  `_repos/`, dot-prefix) are unconditional and cannot be overridden by
  negation patterns in ignore files.
  """
  let _registry: RuleRegistry
  let _file_auth: FileAuth
  let _cwd: String val
  let _package_paths: Array[String val] val

  new val create(
    registry: RuleRegistry,
    file_auth: FileAuth,
    cwd: String val,
    package_paths: Array[String val] val = recover val
      Array[String val] end)
  =>
    _registry = registry
    _file_auth = file_auth
    _cwd = cwd
    _package_paths = package_paths

  fun run(targets: Array[String val] val)
    : (Array[Diagnostic val] ref, ExitCode)
  =>
    """
    Run all enabled rules against the given target paths (files or
    directories). Returns sorted diagnostics and the appropriate exit code.
    """
    let all_diags = Array[Diagnostic val]
    var has_error = false

    // Check for non-existent targets before discovery
    for target in targets.values() do
      let abs_target =
        if Path.is_abs(target) then
          target
        else
          Path.join(_cwd, target)
        end
      let fp = FilePath(_file_auth, abs_target)
      if not fp.exists() then
        all_diags.push(Diagnostic("lint/io-error",
          "target not found: " + target,
          target, 0, 0))
        has_error = true
      end
    end

    let pony_files = _discover(targets)

    // Per-file metadata indexed by absolute path, for reuse in AST phase
    let file_info = Map[String, _FileInfo val]

    // --- Text phase ---
    for path in pony_files.values() do
      let fp = FilePath(_file_auth, path)
      let file = File.open(fp)
      if not file.valid() then
        all_diags.push(Diagnostic("lint/io-error",
          "could not read file: " + path,
          try Path.rel(_cwd, path)? else path end, 0, 0))
        has_error = true
        continue
      end
      let content: String val = file.read_string(file.size())
      file.dispose()
      let source = SourceFile(path, content, _cwd)
      let sup = Suppressions(source)
      let magic_lines = sup.magic_comment_lines()

      // Store for AST phase reuse
      file_info(path) = _FileInfo(source, sup, magic_lines)

      // Collect rule diagnostics, filtering suppressed ones
      for rule in _registry.enabled_text_rules().values() do
        let rule_diags = rule.check(source)
        for diag in rule_diags.values() do
          // Skip diagnostics on magic comment lines
          if magic_lines.contains(diag.line) then continue end
          // Skip suppressed diagnostics
          if sup.is_suppressed(diag.line, diag.rule_id) then continue end
          all_diags.push(diag)
        end
      end

      // Collect suppression error diagnostics (lint/* — never suppressed)
      for err in sup.errors().values() do
        all_diags.push(err)
        has_error = true
      end
    end

    // --- AST phase ---
    if _registry.enabled_ast_rules().size() > 0 then
      // Group files by directory (package)
      let packages = Map[String, Array[String val]]
      for path in pony_files.values() do
        let dir = Path.dir(path)
        let pkg_files =
          try
            packages(dir)?
          else
            let a = Array[String val]
            packages(dir) = a
            a
          end
        pkg_files.push(path)
      end

      // Compile each package and run AST rules
      for (pkg_dir, pkg_file_paths) in packages.pairs() do
        let pkg_fp = FilePath(_file_auth, pkg_dir)
        match ast.Compiler.compile(
          pkg_fp where package_search_paths = _package_paths,
          limit = ast.PassParse)
        | let program: ast.Program val =>
          match program.package()
          | let pkg: ast.Package val =>
            // Run per-module AST rules
            for mod in pkg.modules() do
              let mod_file = mod.file
              try
                let info = file_info(mod_file)?
                let dispatcher =
                  _ASTDispatcher(_registry.enabled_ast_rules(),
                    info.source, info.magic_lines, info.suppressions)
                mod.ast.visit(dispatcher)

                for d in dispatcher.diagnostics().values() do
                  all_diags.push(d)
                end

                // File-naming check (module-level)
                if _registry.is_enabled(FileNaming.id(), FileNaming.category(),
                  FileNaming.default_status())
                then
                  let file_diags =
                    FileNaming.check_module(
                      dispatcher.entities(), info.source)
                  for d in file_diags.values() do
                    if info.magic_lines.contains(d.line) then continue end
                    if info.suppressions.is_suppressed(d.line, d.rule_id) then
                      continue
                    end
                    all_diags.push(d)
                  end
                end

                // Blank-lines between-entity check (module-level)
                if _registry.is_enabled(BlankLines.id(),
                  BlankLines.category(), BlankLines.default_status())
                then
                  let bl_diags =
                    BlankLines.check_module(
                      dispatcher.entities(), info.source)
                  for d in bl_diags.values() do
                    if info.magic_lines.contains(d.line) then continue end
                    if info.suppressions.is_suppressed(d.line, d.rule_id) then
                      continue
                    end
                    all_diags.push(d)
                  end
                end
              end
            end

            // Use first file in the package for diagnostic location
            let first_rel =
              try
                let first_path = pkg_file_paths(0)?
                try file_info(first_path)?.source.rel_path
                else first_path
                end
              else
                pkg_dir
              end

            // Package-naming check (once per package)
            if _registry.is_enabled(PackageNaming.id(),
              PackageNaming.category(), PackageNaming.default_status())
            then
              let pkg_diags =
                PackageNaming.check_package(
                  Path.base(pkg.path), first_rel)
              for d in pkg_diags.values() do
                all_diags.push(d)
              end
            end

            // Package-docstring check (once per package)
            if _registry.is_enabled(PackageDocstring.id(),
              PackageDocstring.category(),
              PackageDocstring.default_status())
            then
              let pkg_base = Path.base(pkg.path)
              // Normalize hyphens to underscores for Pony identifiers
              let norm_name =
                recover val
                  let s = String(pkg_base.size())
                  for ch in pkg_base.values() do
                    if ch == '-' then s.push('_') else s.push(ch) end
                  end
                  s
                end
              let expected_file: String val = norm_name + ".pony"
              var has_pkg_file = false
              var has_docstring = false
              for mod in pkg.modules() do
                let mod_basename = Path.base(mod.file)
                if mod_basename == expected_file then
                  has_pkg_file = true
                  // Check if module AST child 0 is TK_STRING
                  match mod.ast.child()
                  | let first_child: ast.AST =>
                    if first_child.id() == ast.TokenIds.tk_string() then
                      has_docstring = true
                    end
                  end
                  break
                end
              end
              let pd_diags =
                PackageDocstring.check_package(
                  norm_name, has_pkg_file, has_docstring, first_rel)
              for d in pd_diags.values() do
                all_diags.push(d)
              end
            end
          end
        | let errors: Array[ast.Error] val =>
          // AST compilation failed — report as lint/ast-error
          has_error = true
          let rel_dir = try Path.rel(_cwd, pkg_dir)? else pkg_dir end
          for err in errors.values() do
            let err_file =
              match err.file
              | let f: String val =>
                try Path.rel(_cwd, f)? else f end
              else
                rel_dir
              end
            all_diags.push(Diagnostic("lint/ast-error",
              err.msg, err_file, err.position.line(), err.position.column()))
          end
        end
      end
    end

    // Determine exit code
    let exit_code: ExitCode =
      if has_error then
        ExitError
      elseif all_diags.size() > 0 then
        ExitViolations
      else
        ExitSuccess
      end

    // Sort results
    Sort[Array[Diagnostic val], Diagnostic val](all_diags)

    (all_diags, exit_code)

  fun _discover(targets: Array[String val] val): Array[String val] val =>
    """
    Find all .pony files in the given targets. If a target is a file, include
    it directly. If a directory, walk it recursively. Respects `.gitignore`
    (when inside a git repo) and `.ignore` files for path exclusion.
    """
    // Find git repo root from first target's directory
    let root: (String val | None) =
      try
        let first_target = targets(0)?
        let abs_first =
          if Path.is_abs(first_target) then
            first_target
          else
            Path.join(_cwd, first_target)
          end
        let start_dir =
          try
            let fp' = FilePath(_file_auth, abs_first)
            let fi = FileInfo(fp')?
            if fi.directory then abs_first else Path.dir(abs_first) end
          else
            abs_first
          end
        _GitRepoFinder.find_root(_file_auth, start_dir)
      else
        None
      end

    let matcher = IgnoreMatcher(_file_auth, root)

    // Pre-load ignore files from repo root through intermediate directories
    // down to (but not including) each target directory. The target directory
    // itself is loaded during the walk via _FileCollector.apply.
    match root
    | let r: String val =>
      for target in targets.values() do
        let abs_target =
          if Path.is_abs(target) then
            target
          else
            Path.join(_cwd, target)
          end
        let target_dir =
          try
            let fp' = FilePath(_file_auth, abs_target)
            let fi = FileInfo(fp')?
            if fi.directory then abs_target else Path.dir(abs_target) end
          else
            abs_target
          end
        _load_intermediate_ignores(matcher, r, target_dir)
      end
    end

    let collector = _FileCollector(_file_auth, _cwd, matcher)
    for target in targets.values() do
      let abs_target =
        if Path.is_abs(target) then
          target
        else
          Path.join(_cwd, target)
        end
      let fp = FilePath(_file_auth, abs_target)
      try
        let info = FileInfo(fp)?
        if info.file then
          // Explicit file targets are linted regardless of extension
          collector.add(abs_target)
        elseif info.directory then
          fp.walk(collector)
        end
      end
    end
    collector.files()

  fun _load_intermediate_ignores(
    matcher: IgnoreMatcher ref,
    root: String val,
    target_dir: String val)
  =>
    """
    Load ignore files for directories from the repo root through intermediate
    directories down to (but not including) the target directory. When root
    equals target_dir, nothing is loaded here because the walk's first
    `apply` call handles it.
    """
    if root == target_dir then return end
    matcher.load_directory(root)
    let rel =
      try
        Path.rel(root, target_dir)?
      else
        return
      end
    let parts = rel.split_by("/")
    var current: String val = root
    for part in (consume parts).values() do
      current = Path.join(current, part)
      if current == target_dir then break end
      matcher.load_directory(current)
    end

class val _FileInfo
  """
  Per-file metadata stored during text phase for reuse in AST phase.
  """
  let source: SourceFile val
  let suppressions: Suppressions val
  let magic_lines: Set[USize] val

  new val create(
    source': SourceFile val,
    suppressions': Suppressions val,
    magic_lines': Set[USize] val)
  =>
    source = source'
    suppressions = suppressions'
    magic_lines = magic_lines'

class ref _FileCollector is WalkHandler
  """
  Collects .pony file paths during directory walking, respecting hardcoded
  skip rules and `.gitignore`/`.ignore` patterns.
  """
  let _file_auth: FileAuth
  let _cwd: String val
  let _matcher: IgnoreMatcher ref
  let _files: Array[String val]

  new ref create(
    file_auth: FileAuth,
    cwd: String val,
    matcher: IgnoreMatcher ref)
  =>
    _file_auth = file_auth
    _cwd = cwd
    _matcher = matcher
    _files = Array[String val]

  fun ref add(path: String val) =>
    _files.push(path)

  fun ref apply(dir_path: FilePath, dir_entries: Array[String] ref) =>
    // Load ignore files for this directory
    _matcher.load_directory(dir_path.path)

    // Filter out entries we should skip
    var i: USize = 0
    while i < dir_entries.size() do
      try
        let entry = dir_entries(i)?
        if _should_skip(entry) then
          dir_entries.delete(i)?
        else
          let full = Path.join(dir_path.path, entry)
          let fp = FilePath(_file_auth, full)
          try
            let info = FileInfo(fp)?
            if _matcher.is_ignored(full, entry, info.directory) then
              dir_entries.delete(i)?
            else
              if info.file
                and entry.at(".pony", entry.size().isize() - 5)
              then
                _files.push(full)
              end
              i = i + 1
            end
          else
            i = i + 1
          end
        end
      else
        i = i + 1
      end
    end

  fun _should_skip(name: String): Bool =>
    """
    Skip entries named _corral, _repos, or starting with a dot. This
    filters both directories (preventing descent) and files with these
    names.
    """
    (name == "_corral") or (name == "_repos") or
      (try name(0)? == '.' else false end)

  fun ref files(): Array[String val] iso^ =>
    let result = recover iso Array[String val] end
    for f in _files.values() do
      result.push(f)
    end
    consume result
