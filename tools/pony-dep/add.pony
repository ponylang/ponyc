use "files"

actor Add is (ConfigCheckNotify & ConfigWriteNotify)
  """
  Fetches a dependency archive, computes its content hash, and hands the
  entry to a `ConfigAccess` actor for recording in the `pony.deps` file.
  Does not place the dependency in an output directory — that is
  `ResolveDeps`' job.
  """
  let _env: Env
  let _config: (ConfigChecker & ConfigAdder)
  let _notify: AddNotify
  let _name: String val
  let _dep_type: String val
  let _url: String val
  let _ref_name: (String val | None)
  let _documentation_url: (String val | None)
  let _temp_dir: String val

  new create(
    env: Env,
    config: (ConfigChecker & ConfigAdder),
    notify: AddNotify,
    name: String val,
    url: String val,
    work_dir: String val = ".",
    dep_type: String val = "par",
    ref_name: (String val | None) = None,
    documentation_url: (String val | None) = None)
  =>
    _env = env
    _config = config
    _notify = notify
    _name = name
    _dep_type = dep_type
    _url = url
    _ref_name = ref_name
    _documentation_url = documentation_url
    _temp_dir =
      Path.join(work_dir, ".pony-dep-add-" + name)

    if dep_type != "par" then
      notify.add_failed(
        "unsupported dep type '" + dep_type + "'")
      return
    end

    match DepNameValidator(name)
    | let err: String val =>
      notify.add_failed(err)
      return
    end

    config.check_name(name, this)

  be dep_name_available() =>
    """
    Called when the dep name is not already in the config.
    """
    let auth = FileAuth(_env.root)
    let temp_path = FilePath(auth, _temp_dir)
    temp_path.remove()
    Fetch(_env, _AddFetchNotify(this), _url, _temp_dir)

  be dep_name_rejected(message: String val) =>
    _notify.add_failed(message)

  be _fetch_done() =>
    let auth = FileAuth(_env.root)
    let temp_path = FilePath(auth, _temp_dir)

    let hash_bytes: Array[U8] val =
      try
        ContentHash(temp_path)?
      else
        temp_path.remove()
        _notify.add_failed(
          "content hash computation failed")
        return
      end

    temp_path.remove()

    let hash_value: String val =
      "sha256:" + Sha256.hex(hash_bytes)

    _config.add_entry(
      _name,
      _dep_type,
      _url,
      hash_value,
      _ref_name,
      _documentation_url,
      this)

  be dep_added() =>
    _notify.add_succeeded()

  be dep_add_failed(message: String val) =>
    _notify.add_failed(message)

  be _fetch_error(err: FetchError) =>
    let auth = FileAuth(_env.root)
    FilePath(auth, _temp_dir).remove()
    _notify.add_failed(err.message)

actor _AddFetchNotify is FetchNotify
  """
  Routes `FetchNotify` callbacks to the parent `Add` actor.
  """
  let _add: Add

  new create(add: Add) =>
    _add = add

  be fetch_failed(err: FetchError) =>
    _add._fetch_error(err)

  be fetch_succeeded() =>
    _add._fetch_done()
