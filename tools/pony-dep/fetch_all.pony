use "files"
use "runtime_info"

actor FetchAll is ConfigReadNotify
  """
  Reads a `pony.deps` configuration file via a `ConfigAccess` actor and
  fetches all dependencies whose target directories are not already
  present. Fetches run in parallel up to the number of scheduler threads.
  The target directory for a dependency is `<name>@<64 hex chars>` where
  the hex is the sha256 content hash computed from the extracted files.
  """
  let _env: Env
  let _notify: FetchAllNotify
  let _auth: FileAuth
  let _output_dir: String
  let _queue: Array[DepEntry val] = Array[DepEntry val]
  let _max_parallel: USize
  var _in_flight: USize = 0
  var _pending: USize = 0
  var _fetched: USize = 0
  var _skipped: USize = 0
  var _errors: Array[(String val, String val)] iso =
    recover iso Array[(String val, String val)] end

  new create(
    env: Env,
    config: ConfigReader,
    notify: FetchAllNotify,
    output_dir: String)
  =>
    _env = env
    _notify = notify
    _auth = FileAuth(env.root)
    _output_dir = output_dir
    _max_parallel =
      Scheduler.schedulers(SchedulerInfoAuth(env.root)).usize()

    config.read_config(this)

  be config_loaded(cf: ConfigFile val) =>
    """
    Called when the config file has been read and parsed.
    """
    for dep in cf.deps.values() do
      if dep.dep_type != "par" then
        _notify.fetch_all_failed(
          "unsupported dep type '" + dep.dep_type +
            "' for dep '" + dep.name + "'")
        return
      end
    end

    let out_path = FilePath(_auth, _output_dir)
    if not out_path.mkdir() then
      try
        if not FileInfo(out_path)?.directory then
          _notify.fetch_all_failed(
            "output path is not a directory: " + _output_dir)
          return
        end
      else
        _notify.fetch_all_failed(
          "cannot create output directory: " + _output_dir)
        return
      end
    end

    let to_fetch = Array[DepEntry val]
    for dep in cf.deps.values() do
      if dep.hash == "skip" then
        to_fetch.push(dep)
      else
        let hex = _config_hex(dep.hash)
        let target =
          Path.join(_output_dir, dep.name + "@" + hex)
        try
          if FileInfo(FilePath(_auth, target))?.directory then
            _skipped = _skipped + 1
            continue
          end
        end
        to_fetch.push(dep)
      end
    end

    if to_fetch.size() == 0 then
      _notify.fetch_all_complete(
        recover val Array[(String val, String val)] end,
        0,
        _skipped)
      return
    end

    _pending = to_fetch.size()

    for dep in to_fetch.values() do
      _queue.push(dep)
    end
    _launch_queued()

  be config_not_found() =>
    _notify.fetch_all_failed(
      "config file not found")

  be config_error(message: String val) =>
    _notify.fetch_all_failed(message)

  fun ref _launch_queued() =>
    while (_in_flight < _max_parallel) and (_queue.size() > 0) do
      try
        let dep = _queue.shift()?
        let temp_path = FilePath(_auth, _temp_dir(dep.name))
        temp_path.remove()
        Fetch(
          _env,
          _DepFetchNotify(this, dep),
          dep.url,
          _temp_dir(dep.name))
        _in_flight = _in_flight + 1
      else
        _Unreachable()
      end
    end

  be _dep_succeeded(dep: DepEntry val) =>
    _in_flight = _in_flight - 1
    let temp = _temp_dir(dep.name)
    let temp_path = FilePath(_auth, temp)

    let hash_bytes: Array[U8] val =
      try
        ContentHash(temp_path)?
      else
        _record_error(dep.name, "content hash computation failed")
        temp_path.remove()
        _pending = _pending - 1
        _launch_queued()
        _check_done()
        return
      end

    let computed_hex = Sha256.hex(hash_bytes)

    if dep.hash != "skip" then
      let config_hex = _config_hex(dep.hash)
      if computed_hex != config_hex then
        _record_error(
          dep.name,
          "content hash mismatch: expected " + config_hex +
            " got " + computed_hex)
        temp_path.remove()
        _pending = _pending - 1
        _launch_queued()
        _check_done()
        return
      end
    end

    let target_name: String val = dep.name + "@" + computed_hex
    let target: String val = Path.join(_output_dir, target_name)
    let target_path = FilePath(_auth, target)

    try
      if FileInfo(target_path)?.directory then
        temp_path.remove()
        _fetched = _fetched + 1
        _pending = _pending - 1
        _launch_queued()
        _check_done()
        return
      end
    end

    if not temp_path.rename(target_path) then
      _record_error(
        dep.name, "failed to place dependency at " + target)
      temp_path.remove()
      _pending = _pending - 1
      _launch_queued()
      _check_done()
      return
    end

    _fetched = _fetched + 1
    _pending = _pending - 1
    _launch_queued()
    _check_done()

  be _dep_failed(dep: DepEntry val, err: FetchError) =>
    _in_flight = _in_flight - 1
    let temp_path = FilePath(_auth, _temp_dir(dep.name))
    temp_path.remove()
    _record_error(dep.name, err.message)
    _pending = _pending - 1
    _launch_queued()
    _check_done()

  fun ref _record_error(name: String val, message: String val) =>
    _errors.push((name, message))

  fun ref _check_done() =>
    if _pending == 0 then
      var errs: Array[(String val, String val)] iso =
        _errors = recover iso Array[(String val, String val)] end
      _notify.fetch_all_complete(consume errs, _fetched, _skipped)
    end

  fun _temp_dir(dep_name: String val): String val =>
    Path.join(_output_dir, "." + dep_name + ".fetching")

  fun _config_hex(hash: String val): String val =>
    """
    The hex portion of `hash` after the `sha256:` prefix, lowercased.
    """
    recover val
      String .> append(hash.substring(7)) .> lower_in_place()
    end
