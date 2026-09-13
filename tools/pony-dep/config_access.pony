use "files"

actor ConfigAccess is (ConfigChecker & ConfigReader & ConfigAdder)
  """
  Serializes all reads and writes to a `pony.deps` configuration file.
  Other actors communicate with it via message passing rather than
  touching the file directly.
  """
  let _auth: FileAuth
  let _config_path: String val

  new create(auth: FileAuth, config_path: String val) =>
    _auth = auth
    _config_path = config_path

  be read_config(notify: ConfigReadNotify) =>
    """
    Reads and parses the config file. Responds with `config_loaded`,
    `config_not_found`, or `config_error`.
    """
    let file_path = FilePath(_auth, _config_path)
    if not file_path.exists() then
      notify.config_not_found()
      return
    end
    match OpenFile(file_path)
    | let f: File =>
      let data = f.read_string(f.size())
      f.dispose()
      let c: String val = consume data
      match \exhaustive\ ConfigParser(c)
      | let cf: ConfigFile =>
        notify.config_loaded(cf)
      | let e: ConfigError =>
        notify.config_error(
          _config_path + ":" + e.string())
      end
    else
      notify.config_error(
        "cannot read config file: " + _config_path)
    end

  be check_name(
    name: String val,
    notify: ConfigCheckNotify)
  =>
    """
    Reads the config file and checks whether `name` can be added.
    Responds with `dep_name_available` or `dep_name_rejected`.
    """
    let file_path = FilePath(_auth, _config_path)
    if file_path.exists() then
      match OpenFile(file_path)
      | let f: File =>
        let data = f.read_string(f.size())
        f.dispose()
        let c: String val = consume data
        match \exhaustive\ ConfigParser(c)
        | let cf: ConfigFile =>
          for dep in cf.deps.values() do
            if dep.name == name then
              notify.dep_name_rejected(
                "dep '" + name +
                  "' already exists in " + _config_path)
              return
            end
          end
        | let e: ConfigError =>
          notify.dep_name_rejected(
            _config_path + ":" + e.string())
          return
        end
      else
        notify.dep_name_rejected(
          "cannot read config file: " + _config_path)
        return
      end
    end
    notify.dep_name_available()

  be add_entry(
    name: String val,
    dep_type: String val,
    url: String val,
    hash: String val,
    ref_name: (String val | None),
    documentation_url: (String val | None),
    notify: ConfigWriteNotify)
  =>
    """
    Reads the config file, validates no duplicate, appends the entry,
    and writes the result. The entire read-validate-write sequence
    runs in this single behavior.
    """
    let file_path = FilePath(_auth, _config_path)
    let content: (String val | None) =
      if file_path.exists() then
        match OpenFile(file_path)
        | let f: File =>
          let data = f.read_string(f.size())
          f.dispose()
          consume data
        else
          notify.dep_add_failed(
            "cannot read config file: " + _config_path)
          return
        end
      else
        None
      end

    match content
    | let c: String val =>
      match \exhaustive\ ConfigParser(c)
      | let cf: ConfigFile =>
        for dep in cf.deps.values() do
          if dep.name == name then
            notify.dep_add_failed(
              "dep '" + name +
                "' already exists in " + _config_path)
            return
          end
        end
      | let e: ConfigError =>
        notify.dep_add_failed(
          _config_path + ":" + e.string())
        return
      end
    end

    let entry =
      ConfigWriter.format_entry(
        name,
        dep_type,
        url,
        hash,
        ref_name,
        documentation_url)

    let new_content: String val =
      match \exhaustive\ content
      | let c: String val =>
        let trimmed = c.clone() .> rstrip()
        consume trimmed + "\n\n" + entry
      | None => ConfigWriter.new_config_with(entry)
      end

    match CreateFile(file_path)
    | let f: File =>
      if not f.set_length(0) then
        f.dispose()
        notify.dep_add_failed(
          "cannot write config file: " + _config_path)
        return
      end
      if not f.write(new_content) then
        f.dispose()
        notify.dep_add_failed(
          "cannot write config file: " + _config_path)
        return
      end
      f.dispose()
    else
      notify.dep_add_failed(
        "cannot write config file: " + _config_path)
      return
    end
    notify.dep_added()
