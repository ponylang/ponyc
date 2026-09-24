use "files"

primitive _RegressionDb
  """
  Manages regression files for property-based tests.
  """
  fun resolve_dir(env: Env): (FilePath | None) =>
    """
    Resolve the regression directory path. Returns None if PONYCHECK_NO_DB
    is set. Does not create the directory — save() handles that on first
    write.
    """
    if _env_var(env, "PONYCHECK_NO_DB") isnt None then
      return None
    end

    let dir_name =
      match \exhaustive\ _env_var(env, "PONYCHECK_DB_DIR")
      | let s: String => s
      | None => ".ponycheck"
      end

    FilePath(FileAuth(env.root), dir_name)

  fun load(
    dir: FilePath,
    property_name: String,
    logger: PropertyLogger)
    : (Array[_Choice val] val | None)
  =>
    """
    Load a stored regression for the named property.
    Returns None if no file exists, file is corrupt, or read fails.
    Deletes corrupt files.
    """
    let filename: String val = _encode_name(property_name) + ".choices"
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
        "Corrupt regression file for \"" + property_name + "\", removing")
      path.remove()
      None
    end

  fun save(
    dir: FilePath,
    property_name: String,
    choices: Array[_Choice val] val,
    logger: PropertyLogger)
  =>
    """
    Write a failing choice sequence to disk. Creates the directory if
    needed. Best-effort — logs on failure.
    """
    if not dir.exists() then
      if not dir.mkdir() then
        logger.log(
          "Cannot create regression directory, " +
            "regression not saved for \"" + property_name + "\"")
        return
      end
    end

    let filename: String val = _encode_name(property_name) + ".choices"
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
    logger: PropertyLogger)
  =>
    """
    Delete the regression file for a property.
    """
    let filename: String val = _encode_name(property_name) + ".choices"
    try
      dir.join(filename)?.remove()
    end

  fun _encode_name(name: String): String =>
    """
    Percent-encode property name for use as a filename.
    Lowercases [A-Z] to avoid collisions on case-insensitive filesystems.
    Keeps [a-z0-9._-], encodes everything else.
    """
    let buf = recover iso String(name.size()) end
    for byte in name.values() do
      if (byte >= 'A') and (byte <= 'Z') then
        buf.push(byte + ('a' - 'A'))
      elseif ((byte >= 'a') and (byte <= 'z')) or
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
