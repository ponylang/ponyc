use "collections"
use "files"

class val Linter
  """
  Orchestrates the lint pipeline: discover files, load and check each one
  against enabled rules, apply suppressions, and produce sorted diagnostics.

  File discovery recursively finds .pony files in given targets, skipping
  _corral/, _repos/, and directories starting with a dot.
  """
  let _registry: RuleRegistry
  let _file_auth: FileAuth
  let _cwd: String val

  new val create(
    registry: RuleRegistry,
    file_auth: FileAuth,
    cwd: String val)
  =>
    _registry = registry
    _file_auth = file_auth
    _cwd = cwd

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

    // Determine exit code before consuming
    let diag_count = all_diags.size()
    let exit_code: ExitCode =
      if has_error then
        ExitError
      elseif diag_count > 0 then
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
    it directly. If a directory, walk it recursively.
    """
    let collector = _FileCollector(_file_auth, _cwd)
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

class ref _FileCollector is WalkHandler
  """Collects .pony file paths during directory walking."""
  let _file_auth: FileAuth
  let _cwd: String val
  let _files: Array[String val]

  new ref create(file_auth: FileAuth, cwd: String val) =>
    _file_auth = file_auth
    _cwd = cwd
    _files = Array[String val]

  fun ref add(path: String val) =>
    _files.push(path)

  fun ref apply(dir_path: FilePath, dir_entries: Array[String] ref) =>
    // Filter out entries we should skip
    var i: USize = 0
    while i < dir_entries.size() do
      try
        let entry = dir_entries(i)?
        if _should_skip(entry) then
          dir_entries.delete(i)?
        else
          // Check if entry is a .pony file
          let full = Path.join(dir_path.path, entry)
          let fp = FilePath(_file_auth, full)
          try
            let info = FileInfo(fp)?
            if info.file and entry.at(".pony", entry.size().isize() - 5) then
              _files.push(full)
            end
          end
          i = i + 1
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
