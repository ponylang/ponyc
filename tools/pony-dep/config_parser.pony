use "collections"

class val ConfigError
  """
  A parse error from a `pony.deps` configuration file. Carries the
  1-indexed line number where the error was detected and a description.
  Line 0 means the error applies to the file as a whole.
  """
  let line: USize
  let message: String val

  new val _create(line': USize, message': String val) =>
    line = line'
    message = message'

  fun string(): String val =>
    line.string() + ": " + message

class val DepEntry
  """
  A single dependency parsed from a `pony.deps` file. The `hash` field
  is either `sha256:<64 hex chars>` (Merkle tree root) or `skip`.
  """
  let name: String val
  let dep_type: String val
  let url: String val
  let ref_name: String val
  let hash: String val
  let documentation_url: (String val | None)

  new val _create(
    name': String val,
    dep_type': String val,
    url': String val,
    ref_name': String val,
    hash': String val,
    documentation_url': (String val | None))
  =>
    name = name'
    dep_type = dep_type'
    url = url'
    ref_name = ref_name'
    hash = hash'
    documentation_url = documentation_url'

class val ConfigFile
  """
  A parsed `pony.deps` configuration file. Dependencies appear in
  declaration order.
  """
  let version: U64
  let deps: Array[DepEntry val] val

  new val _create(version': U64, deps': Array[DepEntry val] val) =>
    version = version'
    deps = deps'

primitive ConfigParser
  """
  Parses `pony.deps` configuration file content. Returns a `ConfigFile`
  on success or a `ConfigError` describing the first problem found.

  Supports format version 1.
  """
  fun apply(content: String val): (ConfigFile | ConfigError) =>
    """
    Returns `ConfigFile` on success or `ConfigError` on the first
    problem found.
    """
    let lines = content.split_by("\n")
    var version: (U64 | None) = None
    var deps: Array[DepEntry val] iso =
      recover iso Array[DepEntry val] end
    var dep_names: Array[String val] = Array[String val]

    var in_dep: Bool = false
    var dep_name: String val = ""
    var dep_start_line: USize = 0
    var fields: Map[String val, String val] = Map[String val, String val]

    var line_num: USize = 0
    for raw_line in (consume lines).values() do
      line_num = line_num + 1
      let line: String ref = raw_line.clone()
      line.strip()

      if line.size() == 0 then continue end

      try
        if line(0)? == '#' then continue end
      else
        _Unreachable()
      end

      if version is None then
        if not (line.at("version ", 0) or line.at("version\t", 0)) then
          return ConfigError._create(
            line_num, "expected 'version <N>' as the first line")
        end
        var ver_str: String iso = line.substring(8)
        ver_str.strip()
        let ver_val: String val = consume ver_str
        match \exhaustive\ _parse_u64(ver_val)
        | let n: U64 =>
          if n == 0 then
            return ConfigError._create(
              line_num, "invalid version number 0")
          end
          if n > 1 then
            return ConfigError._create(
              line_num,
              "unsupported config version " + n.string() +
                " (this tool supports version 1)")
          end
          version = n
        | None =>
          return ConfigError._create(
            line_num, "invalid version number")
        end
        continue
      end

      if in_dep then
        if line == "end" then
          let t =
            try fields("type")?
            else
              return ConfigError._create(
                line_num,
                "dep '" + dep_name +
                  "' missing required field 'type'")
            end
          let u =
            try fields("url")?
            else
              return ConfigError._create(
                line_num,
                "dep '" + dep_name +
                  "' missing required field 'url'")
            end
          let r =
            try fields("ref")?
            else
              return ConfigError._create(
                line_num,
                "dep '" + dep_name +
                  "' missing required field 'ref'")
            end
          let h =
            try fields("hash")?
            else
              return ConfigError._create(
                line_num,
                "dep '" + dep_name +
                  "' missing required field 'hash'")
            end
          let doc_url: (String val | None) =
            try fields("documentation_url")? else None end

          deps.push(DepEntry._create(dep_name, t, u, r, h, doc_url))
          in_dep = false
          dep_name = ""
          fields = Map[String val, String val]
          continue
        end

        if line.at("end ", 0) or line.at("end\t", 0) then
          return ConfigError._create(
            line_num, "unexpected content after 'end'")
        end

        if (line == "dep") or
          line.at("dep ", 0) or line.at("dep\t", 0)
        then
          return ConfigError._create(
            line_num, "nested 'dep' block not allowed")
        end

        let space_idx =
          try line.find(" ")?
          else
            return ConfigError._create(
              line_num,
              "field missing value: '" + line.clone() + "'")
          end

        let key: String val = line.substring(0, space_idx)
        var value_iso: String iso = line.substring(space_idx + 1)
        value_iso.strip()
        let value: String val = consume value_iso

        if value.size() == 0 then
          return ConfigError._create(
            line_num, "field '" + key + "' has no value")
        end

        if fields.contains(key) then
          return ConfigError._create(
            line_num,
            "duplicate field '" + key +
              "' in dep '" + dep_name + "'")
        end

        if key == "hash" then
          match _validate_hash(value, line_num)
          | let e: ConfigError => return e
          end
        end

        fields(key) = value
      else
        if (line == "dep") or
          line.at("dep ", 0) or line.at("dep\t", 0)
        then
          var name_iso: String iso = line.substring(4)
          name_iso.strip()
          let name_val: String val = consume name_iso

          if name_val.size() == 0 then
            return ConfigError._create(
              line_num, "'dep' requires a name")
          end

          try
            name_val.find(" ")?
            return ConfigError._create(
              line_num, "dep name must be a single token")
          end
          try
            name_val.find("\t")?
            return ConfigError._create(
              line_num, "dep name must be a single token")
          end

          for existing in dep_names.values() do
            if existing == name_val then
              return ConfigError._create(
                line_num,
                "duplicate dep name '" + name_val + "'")
            end
          end

          dep_names.push(name_val)
          dep_name = name_val
          dep_start_line = line_num
          in_dep = true
          continue
        end

        if (line == "end") or
          line.at("end ", 0) or line.at("end\t", 0)
        then
          return ConfigError._create(
            line_num, "'end' without a matching 'dep'")
        end

        if (line == "version") or
          line.at("version ", 0) or line.at("version\t", 0)
        then
          return ConfigError._create(
            line_num,
            "'version' can only appear as the first line")
        end

        return ConfigError._create(
          line_num, "unexpected line at top level")
      end
    end

    if in_dep then
      return ConfigError._create(
        dep_start_line,
        "dep '" + dep_name + "' has no closing 'end'")
    end

    match \exhaustive\ version
    | let v: U64 =>
      ConfigFile._create(v, consume deps)
    | None =>
      ConfigError._create(USize(0), "missing 'version' line")
    end

  fun _parse_u64(s: String val): (U64 | None) =>
    if s.size() == 0 then return None end
    var result: U64 = 0
    var i: USize = 0
    while i < s.size() do
      let c = try s(i)? else _Unreachable(); return None end
      if (c < '0') or (c > '9') then return None end
      result =
        try
          result.mul_partial(10)?.add_partial((c - '0').u64())?
        else
          return None
        end
      i = i + 1
    end
    result

  fun _validate_hash(
    value: String val,
    line_num: USize)
    : (None | ConfigError)
  =>
    if value == "skip" then return None end

    if not value.at("sha256:", 0) then
      return ConfigError._create(
        line_num,
        "hash must be 'skip' or 'sha256:<64 hex chars>'")
    end

    let hex: String val = value.substring(7)
    if hex.size() != 64 then
      return ConfigError._create(
        line_num,
        "sha256 hash must be exactly 64 hex characters, got " +
          hex.size().string())
    end

    var i: USize = 0
    while i < hex.size() do
      let c = try hex(i)? else _Unreachable(); return None end
      let is_hex =
        ((c >= '0') and (c <= '9')) or
          ((c >= 'a') and (c <= 'f')) or
          ((c >= 'A') and (c <= 'F'))
      if not is_hex then
        return ConfigError._create(
          line_num, "invalid hex character in hash")
      end
      i = i + 1
    end
    None
