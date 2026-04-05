use "files"

primitive _GitRepoFinder
  """
  Walks up the directory tree from a starting point looking for a `.git`
  entry (directory or file) to locate the git repository root.
  """

  fun find_root(file_auth: FileAuth, start: String val)
    : (String val | None)
  =>
    """
    Return the absolute path of the git repository root containing `start`,
    or `None` if no `.git` entry is found before reaching the filesystem
    root.
    """
    var dir: String val = start
    while true do
      let git_path = Path.join(dir, ".git")
      let fp = FilePath(file_auth, git_path)
      if fp.exists() then
        return dir
      end
      let parent = Path.dir(dir)
      if parent == dir then
        return None
      end
      dir = parent
    end
    None

class ref IgnoreMatcher
  """
  Evaluates paths against `.gitignore` and `.ignore` rules accumulated from
  the directory hierarchy.

  Rules are loaded per-directory via `load_directory` and evaluated
  last-to-first so that rules from deeper directories override those from
  parent directories, and later lines within a file override earlier ones.

  File precedence per directory (last-loaded = highest precedence):
  1. `.gitignore` (only when inside a git repository)
  2. `.ignore` (always loaded)
  """
  let _file_auth: FileAuth
  let _root: String val
  let _in_git_repo: Bool
  let _rules: Array[(IgnorePattern val, String val)]
  let _errors: Array[(String val, String val)]

  new ref create(file_auth: FileAuth, root: (String val | None)) =>
    """
    Create a matcher for the given repository root. Pass the git repo root
    path to enable `.gitignore` loading, or `None` to only load `.ignore`
    files.
    """
    _file_auth = file_auth
    match root
    | let r: String val =>
      _root = r
      _in_git_repo = true
    else
      _root = ""
      _in_git_repo = false
    end
    _rules = Array[(IgnorePattern val, String val)]
    _errors = Array[(String val, String val)]

  fun ref load_directory(dir_path: String val) =>
    """
    Read `.gitignore` (if in a git repo) and `.ignore` from the given
    directory, parsing each line into rules. Later-loaded rules take
    precedence during evaluation.
    """
    if _in_git_repo then
      _load_file(Path.join(dir_path, ".gitignore"), dir_path)
    end
    _load_file(Path.join(dir_path, ".ignore"), dir_path)

  fun errors(): this->Array[(String val, String val)] =>
    """
    Return errors accumulated during `load_directory` calls. Each entry is
    a `(message, file_path)` tuple. Call `clear_errors` after draining.
    """
    _errors

  fun ref clear_errors() =>
    """
    Remove all accumulated errors. Call after draining `errors()`.
    """
    _errors.clear()

  fun ref _load_file(file_path: String val, base_dir: String val) =>
    """
    Parse all lines from an ignore file and append the resulting rules.

    Rejects files that cannot be opened or are larger than 64 KB to prevent
    unexpected memory consumption, especially with hierarchical ignore
    loading where each directory can have its own `.gitignore` and `.ignore`
    files.
    """
    let fp = FilePath(_file_auth, file_path)
    if not fp.exists() then return end
    let file = File.open(fp)
    if not file.valid() then
      _errors.push((
        "could not open ignore file: " + file_path,
        file_path))
      return
    end
    let size = file.size()
    if size > _max_ignore_file_size() then
      file.dispose()
      _errors.push((
        "ignore file too large (" + size.string() + " bytes, max "
          + _max_ignore_file_size().string() + "): " + file_path,
        file_path))
      return
    end
    let content: String val = file.read_string(size)
    file.dispose()
    // Normalize line endings and parse
    let clean_content: String val =
      if content.contains("\r") then
        let s = content.clone()
        s.remove("\r")
        consume s
      else
        content
      end
    for line in clean_content.split_by("\n").values() do
      match PatternParser(line)
      | let pat: IgnorePattern val =>
        _rules.push((pat, base_dir))
      end
    end

  fun is_ignored(
    abs_path: String val,
    entry_name: String val,
    is_dir: Bool)
    : Bool
  =>
    """
    Return true if the entry at `abs_path` should be ignored based on
    accumulated rules. Evaluates rules last-to-first; the first matching rule
    wins. A negated match means the entry is NOT ignored.
    """
    var i = _rules.size()
    while i > 0 do
      i = i - 1
      try
        (let pat, let base_dir) = _rules(i)?

        // A rule only applies to entries within its base directory tree
        if not _in_scope(abs_path, base_dir) then continue end

        // Skip directory-only patterns for non-directories
        if pat.dir_only and (not is_dir) then continue end

        let matched =
          if pat.anchored then
            // Match against path relative to the rule's base directory,
            // normalized to forward slashes for GlobMatch
            let rel =
              try
                let r = Path.rel(base_dir, abs_path)?
                ifdef windows then
                  let s = r.clone()
                  s.replace("\\", "/")
                  consume s
                else
                  r
                end
              else
                abs_path
              end
            GlobMatch.matches(pat.pattern, rel)
          else
            // Match against just the entry name (basename)
            GlobMatch.matches(pat.pattern, entry_name)
          end

        if matched then
          return not pat.negated
        end
      end
    end
    false

  fun _in_scope(abs_path: String val, base_dir: String val): Bool =>
    """
    Return true if `abs_path` is within the `base_dir` subtree. A rule from
    a `.gitignore` in `base_dir` only applies to entries at or below that
    directory.
    """
    if abs_path.at(base_dir) then
      let blen = base_dir.size()
      // Exact match or path continues with a separator
      (abs_path.size() == blen)
        or (try Path.is_sep(abs_path(blen)?) else false end)
    else
      false
    end

  fun _max_ignore_file_size(): USize => 65_536
