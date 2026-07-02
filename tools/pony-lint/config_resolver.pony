use "collections"
use "files"

class ref ConfigResolver
  """
  Resolves per-directory rule status by merging the config hierarchy.

  Accumulates per-directory overrides during the file-discovery walk via
  `add_directory()`, then resolves the effective config for any directory
  via `config_for()`. The hierarchy root is the directory containing the
  root config file (or CWD if no root config exists).

  Invariant: omission in a directory config means "defer to parent,"
  not "reset to default."
  """
  let _root_config: LintConfig
  let _dir_overrides: Map[String, Map[String, RuleStatus] val]
  let _root_dir: String val
  let _cache: Map[String, LintConfig val]

  new ref create(root_config: LintConfig, root_dir: String val) =>
    _root_config = root_config
    _dir_overrides = Map[String, Map[String, RuleStatus] val]
    _root_dir = root_dir
    _cache = Map[String, LintConfig val]

  fun ref add_directory(
    dir: String val,
    rules: Map[String, RuleStatus] val)
  =>
    """
    Register an override config for a directory. Takes pre-parsed config
    data — the caller handles I/O and parsing.

    Skips the hierarchy root directory (its config is already in the root
    LintConfig) and skips directories already registered (idempotent).
    This guard is centralized here rather than in each caller to prevent
    double-loading the root's config, which would corrupt rule-specific
    entries via category cleaning.
    """
    if (dir == _root_dir) or _dir_overrides.contains(dir) then return end
    _dir_overrides(dir) = rules

  fun ref config_for(dir: String val): LintConfig val =>
    """
    Return the effective LintConfig for a directory by merging the
    hierarchy from root to the given directory. Results are cached
    per directory path.
    """
    try return _cache(dir)? end
    // Walk up from dir to root, collecting ancestor directories
    // that have overrides (in leaf-to-root order)
    let ancestors = Array[String val]
    var current = dir
    while true do
      if _dir_overrides.contains(current) then
        ancestors.push(current)
      end
      if current == _root_dir then break end
      let parent = Path.dir(current)
      if parent == current then break end
      current = parent
    end
    // Merge root-to-leaf: start with root config,
    // then merge each ancestor's overrides in root-to-leaf order
    var merged: LintConfig val = _root_config
    // ancestors is leaf-to-root, so iterate in reverse
    var i = ancestors.size()
    while i > 0 do
      i = i - 1
      try
        let ancestor_dir = ancestors(i)?
        let overrides = _dir_overrides(ancestor_dir)?
        merged = LintConfig.merge(merged, overrides)
      end
    end
    _cache(dir) = merged
    merged
