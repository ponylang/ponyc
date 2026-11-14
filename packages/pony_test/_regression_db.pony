use "files"

primitive _RegressionDb
  """
  Manages regression files for property-based tests.

  Supports multiple regressions per property for multi-failure mode.
  The bare property name maps to `encoded.choices`; indexed variants
  map to `encoded~2.choices`, `encoded~3.choices`, etc. The `~`
  separator is appended after encoding, so it cannot collide with any
  encoded property name.
  """
  fun resolve_dir(env: Env): (FilePath | None) =>
    """
    Resolve the regression directory path. Returns None if PONYTEST_NO_DB
    is set. Does not create the directory — save() handles that on first
    write.
    """
    if _env_var(env, "PONYTEST_NO_DB") isnt None then
      return None
    end

    let dir_name =
      match \exhaustive\ _env_var(env, "PONYTEST_DB_DIR")
      | let s: String => s
      | None => ".ponytest"
      end

    FilePath(FileAuth(env.root), dir_name)

  fun load(
    dir: FilePath,
    property_name: String,
    logger: _PropertyLogger)
    : Array[Array[_Choice val] val] val
  =>
    """
    Load all stored regressions for the named property.
    Returns an empty array if none exist.
    """
    let result = recover iso Array[Array[_Choice val] val] end
    let encoded = _encode_name(property_name)
    let suffix: String val = ".choices"

    match _load_one(dir, encoded + suffix, property_name, logger)
    | let choices: Array[_Choice val] val =>
      result.push(choices)
    end

    let prefix: String val = encoded + "~"
    try
      let d = Directory(dir)?
      let entries = d.entries()?
      for entry in (consume entries).values() do
        if
          entry.at(prefix) and entry.at(
            suffix, entry.size().isize() - suffix.size().isize())
        then
          let mid =
            entry.substring(
              prefix.size().isize(),
              entry.size().isize() - suffix.size().isize())
          try mid.usize(10)? else continue end
          match _load_one(dir, entry, property_name, logger)
          | let choices: Array[_Choice val] val =>
            result.push(choices)
          end
        end
      end
    end

    consume result

  fun _load_one(
    dir: FilePath,
    filename: String,
    property_name: String,
    logger: _PropertyLogger)
    : (Array[_Choice val] val | None)
  =>
    let path =
      try
        dir.join(filename)?
      else
        return None
      end

    if not path.exists() then return None end

    let data =
      try
        let file = OpenFile(path) as File
        let contents = file.read_string(file.size())
        file.dispose()
        consume contents
      else
        return None
      end

    match \exhaustive\ _ChoiceSerializer.deserialize(consume data)
    | let choices: Array[_Choice val] val =>
      choices
    | None =>
      logger.log(
        "Corrupt regression file for \"" + property_name +
          "\", removing")
      path.remove()
      None
    end

  fun save(
    dir: FilePath,
    property_name: String,
    choices: Array[_Choice val] val,
    logger: _PropertyLogger)
  =>
    """
    Write a failing choice sequence to the bare (first/only) regression
    file. Creates the directory if needed. Best-effort — logs on failure.
    """
    let filename: String val = _encode_name(property_name) + ".choices"
    _write(dir, filename, property_name, choices, logger)

  fun save_indexed(
    dir: FilePath,
    property_name: String,
    index: USize,
    choices: Array[_Choice val] val,
    logger: _PropertyLogger)
  =>
    """
    Write a failing choice sequence to an indexed regression file.
    `index` is the 1-based index (2, 3, ...) appended as `~N` after
    encoding the property name.
    """
    let filename: String val =
      _encode_name(property_name) + "~" + index.string() + ".choices"
    _write(dir, filename, property_name, choices, logger)

  fun _write(
    dir: FilePath,
    filename: String val,
    property_name: String,
    choices: Array[_Choice val] val,
    logger: _PropertyLogger)
  =>
    if not dir.exists() then
      if not dir.mkdir() then
        logger.log(
          "Cannot create regression directory, " +
            "regression not saved for \"" + property_name + "\"")
        return
      end
    end

    let path =
      try
        dir.join(filename)?
      else
        logger.log(
          "Cannot construct path for regression file, " +
            "regression not saved for \"" + property_name + "\"")
        return
      end

    try
      let file = CreateFile(path) as File
      let data = _ChoiceSerializer.serialize(choices)
      if not file.write(consume data) then
        logger.log(
          "Cannot write regression file for \"" + property_name + "\"")
      end
      file.dispose()
    else
      logger.log(
        "Cannot write regression file for \"" + property_name + "\"")
    end

  fun clear(
    dir: FilePath,
    property_name: String,
    logger: _PropertyLogger)
  =>
    """
    Delete all regression files for a property (bare and indexed).
    """
    let encoded = _encode_name(property_name)
    let suffix: String val = ".choices"

    try dir.join(encoded + suffix)?.remove() end

    let prefix: String val = encoded + "~"
    try
      let d = Directory(dir)?
      let entries = d.entries()?
      for entry in (consume entries).values() do
        if
          entry.at(prefix) and entry.at(
            suffix, entry.size().isize() - suffix.size().isize())
        then
          let mid =
            entry.substring(
              prefix.size().isize(),
              entry.size().isize() - suffix.size().isize())
          try mid.usize(10)? else continue end
          try dir.join(entry)?.remove() end
        end
      end
    end

  fun _encode_name(name: String): String =>
    """
    Percent-encode property name for use as a filename.
    Keeps [a-z0-9._-] literally, encodes everything else as `%XX`.
    Uppercase letters are encoded (A → `%41`) rather than folded to
    lowercase, so names differing only in case produce distinct files
    while avoiding bare uppercase in filenames (safe on case-insensitive
    filesystems).
    """
    let buf = recover iso String(name.size()) end
    for byte in name.values() do
      if ((byte >= 'a') and (byte <= 'z')) or
        ((byte >= '0') and (byte <= '9')) or
        (byte == '.') or
        (byte == '_') or
        (byte == '-')
      then
        buf.push(byte)
      else
        buf.append("%")
        let hi = (byte >> 4) and 0x0F
        let lo = byte and 0x0F
        buf.push(_hex_char(hi))
        buf.push(_hex_char(lo))
      end
    end
    consume buf

  fun _hex_char(nibble: U8): U8 =>
    if nibble < 10 then
      '0' + nibble
    else
      'A' + (nibble - 10)
    end

  fun _env_var(env: Env, key: String): (String | None) =>
    """
    Scan env.vars for a KEY=VALUE entry.
    """
    let prefix: String val = key + "="
    for entry in env.vars.values() do
      if entry.at(prefix) then
        return entry.substring(prefix.size().isize())
      end
    end
    None
