use "collections"
use "files"
use json = "json"

class val ConfigError
  """
  Describes why configuration loading failed.
  """
  let message: String val

  new val create(message': String val) =>
    message = message'

class val LintConfig
  """
  Configuration for which lint rules are enabled or disabled.

  Effective rule status is determined by precedence:
  1. CLI `--disable` flags (highest)
  2. `.pony-lint.json` file settings
  3. Rule default status (lowest)

  Both rule-specific IDs (`style/line-length`) and category names (`style`)
  are supported at all levels.
  """
  let _cli_disabled: Set[String] val
  let _file_rules: Map[String, RuleStatus] val

  new val create(
    cli_disabled: Set[String] val,
    file_rules: Map[String, RuleStatus] val)
  =>
    _cli_disabled = cli_disabled
    _file_rules = file_rules

  new val default() =>
    """
    Create a config with no overrides — all rules use their defaults.
    """
    _cli_disabled = recover val Set[String] end
    _file_rules = recover val Map[String, RuleStatus] end

  fun rule_status(
    rule_id: String,
    category: String,
    default_status: RuleStatus)
    : RuleStatus
  =>
    """
    Returns the effective status for a rule, applying the precedence chain:
    CLI disable > file config (rule-specific) > file config (category) >
    rule default.
    """
    // CLI disable has highest precedence
    if _cli_disabled.contains(rule_id) or _cli_disabled.contains(category) then
      return RuleOff
    end
    // File config: rule-specific overrides category
    try
      return _file_rules(rule_id)?
    end
    try
      return _file_rules(category)?
    end
    // Fall through to default
    default_status

primitive ConfigLoader
  """
  Loads lint configuration from CLI arguments and/or a `.pony-lint.json` file.
  """
  fun from_cli(
    cli_disabled: Array[String] val,
    config_path: (String | None),
    file_auth: FileAuth)
    : (LintConfig | ConfigError)
  =>
    """
    Build a `LintConfig` from CLI disable flags and an optional config file.
    If `config_path` is None, auto-discovers the config file.
    """
    let disabled =
      recover val
        let s = Set[String]
        for item in cli_disabled.values() do
          s.set(item)
        end
        s
      end
    let file_rules =
      match \exhaustive\ config_path
      | let path: String =>
        let fp = FilePath(file_auth, path)
        if not fp.exists() then
          return ConfigError("config file not found: " + path)
        end
        match \exhaustive\ _load_file(fp)
        | let rules: Map[String, RuleStatus] val => rules
        | let err: ConfigError => return err
        end
      | None =>
        match \exhaustive\ _discover(file_auth)
        | let fp: FilePath =>
          match \exhaustive\ _load_file(fp)
          | let rules: Map[String, RuleStatus] val => rules
          | let err: ConfigError => return err
          end
        | None =>
          recover val Map[String, RuleStatus] end
        end
      end
    LintConfig(disabled, file_rules)

  fun _discover(file_auth: FileAuth): (FilePath | None) =>
    """
    Discover `.pony-lint.json` by checking CWD first, then walking up
    to find `corral.json` and checking its directory.
    """
    let cwd = Path.cwd()
    // Check CWD first
    let cwd_config = Path.join(cwd, ".pony-lint.json")
    let cwd_fp = FilePath(file_auth, cwd_config)
    if cwd_fp.exists() then
      return cwd_fp
    end
    // Walk up looking for corral.json
    var dir = cwd
    while true do
      let corral_path = Path.join(dir, "corral.json")
      let corral_fp = FilePath(file_auth, corral_path)
      if corral_fp.exists() then
        let config_path = Path.join(dir, ".pony-lint.json")
        let config_fp = FilePath(file_auth, config_path)
        if config_fp.exists() then
          return config_fp
        end
        return None
      end
      let parent = Path.dir(dir)
      if parent == dir then
        break
      end
      dir = parent
    end
    None

  fun _load_file(fp: FilePath): (Map[String, RuleStatus] val | ConfigError) =>
    """
    Parse a `.pony-lint.json` file into rule status overrides.
    """
    let file = File.open(fp)
    if not file.valid() then
      return ConfigError("could not open config file: " + fp.path)
    end
    let content: String val = file.read_string(file.size())
    file.dispose()
    parse(content)

  fun parse(content: String val)
    : (Map[String, RuleStatus] val | ConfigError)
  =>
    """
    Parse JSON content into rule status overrides.
    """
    match \exhaustive\ json.JsonParser.parse(content)
    | let value: json.JsonValue =>
      match \exhaustive\ value
      | let obj: json.JsonObject =>
        try
          match obj("rules")?
          | let rules_obj: json.JsonObject =>
            _parse_rules(rules_obj)
          else
            ConfigError("\"rules\" must be an object")
          end
        else
          // No "rules" key — valid but empty config
          recover val Map[String, RuleStatus] end
        end
      else
        ConfigError("config file must contain a JSON object")
      end
    | let err: json.JsonParseError =>
      ConfigError("malformed JSON in config file: " + err.string())
    end

  fun _parse_rules(rules: json.JsonObject)
    : (Map[String, RuleStatus] val | ConfigError)
  =>
    recover val
      let result = Map[String, RuleStatus]
      for (key, value) in rules.pairs() do
        match \exhaustive\ value
        | let s: String =>
          if s == "on" then
            result(key) = RuleOn
          elseif s == "off" then
            result(key) = RuleOff
          else
            return ConfigError(
              "invalid rule status '" + s + "' for '" + key
                + "': must be \"on\" or \"off\"")
          end
        else
          return ConfigError(
            "rule status for '" + key + "' must be a string")
        end
      end
      result
    end
