use "collections"
use "files"
use "runtime_info"

actor ResolveDeps
  """
  Fetches all dependencies transitively. Reads `pony.deps` files, deduplicates
  dependencies across all configs, and fetches each dependency at most once.
  After placing a dependency, reads its `pony.deps` file if present and adds
  those dependencies to the work queue, repeating until the full closure is
  present in the output directory.

  All dependencies land flat in the same output directory — no two fetch
  operations ever target the same dependency.
  """
  let _env: Env
  let _notify: ResolveNotify
  let _auth: FileAuth
  let _output_dir: String
  let _max_parallel: USize
  let _dep_queue: Array[DepEntry val] = Array[DepEntry val]
  let _seen_deps: HashSet[String, HashEq[String]] =
    _seen_deps.create()
  let _in_flight: HashSet[String, HashEq[String]] =
    _in_flight.create()
  let _pending_configs: HashSet[String, HashEq[String]] =
    _pending_configs.create()
  let _processed_configs: HashSet[String, HashEq[String]] =
    _processed_configs.create()
  var _fetched: USize = 0
  var _skipped: USize = 0
  var _errors: Array[(String val, String val)] iso =
    recover iso Array[(String val, String val)] end

  new create(
    env: Env,
    config_path: String,
    notify: ResolveNotify,
    output_dir: String)
  =>
    """
    Resolve all dependencies starting from `config_path`. Results are
    reported through `notify` once the full transitive closure has been
    fetched or when the root config cannot be read.
    """
    _env = env
    _notify = notify
    _auth = FileAuth(env.root)
    _output_dir = output_dir
    _max_parallel =
      Scheduler.schedulers(SchedulerInfoAuth(env.root)).usize()

    _processed_configs.set(config_path)
    _pending_configs.set(config_path)
    ConfigAccess(_auth, config_path).read_config(
      _ResolveConfigNotify(this, config_path, true))

  be _config_loaded(config_path: String, is_root: Bool, cf: ConfigFile val) =>
    _pending_configs.unset(config_path)

    if is_root then
      for dep in cf.deps.values() do
        if dep.dep_type != "par" then
          _notify.resolve_failed(
            "unsupported dep type '" + dep.dep_type +
              "' for dep '" + dep.name + "'")
          return
        end
      end

      let out_path = FilePath(_auth, _output_dir)
      if not out_path.mkdir() then
        try
          if not FileInfo(out_path)?.directory then
            _notify.resolve_failed(
              "output path is not a directory: " + _output_dir)
            return
          end
        else
          _notify.resolve_failed(
            "cannot create output directory: " + _output_dir)
          return
        end
      end
    end

    for dep in cf.deps.values() do
      if dep.dep_type != "par" then
        let dep_dir = Path.base(Path.dir(config_path))
        _errors.push((dep_dir,
          "unsupported dep type '" + dep.dep_type +
            "' for dep '" + dep.name + "'"))
        continue
      end

      let target_name = _dep_target(dep)
      if _seen_deps.contains(target_name) then continue end
      _seen_deps.set(target_name)

      if dep.hash != "skip" then
        let hex = _config_hex(dep.hash)
        let target = Path.join(_output_dir, dep.name + "@" + hex)
        try
          if FileInfo(FilePath(_auth, target))?.directory then
            _skipped = _skipped + 1
            _check_transitive(target)
            continue
          end
        end
      end

      _dep_queue.push(dep)
    end

    _launch_queued()
    _check_done()

  be _config_not_found(config_path: String, is_root: Bool) =>
    _pending_configs.unset(config_path)
    if is_root then
      _notify.resolve_failed("config file not found")
    else
      let dep_dir = Path.base(Path.dir(config_path))
      _errors.push((dep_dir, "config file not found"))
      _launch_queued()
      _check_done()
    end

  be _config_error(config_path: String, is_root: Bool, message: String val) =>
    _pending_configs.unset(config_path)
    if is_root then
      _notify.resolve_failed(message)
    else
      let dep_dir = Path.base(Path.dir(config_path))
      _errors.push((dep_dir, message))
      _launch_queued()
      _check_done()
    end

  fun ref _launch_queued() =>
    while (_in_flight.size() < _max_parallel) and (_dep_queue.size() > 0) do
      try
        let dep = _dep_queue.shift()?
        let token = _dep_target(dep)
        let temp = _temp_dir(token)
        let temp_path = FilePath(_auth, temp)
        temp_path.remove()
        Fetch(
          _env,
          _ResolveFetchNotify(this, dep),
          dep.url,
          temp)
        _in_flight.set(token)
      else
        _Unreachable()
      end
    end

  be _dep_succeeded(dep: DepEntry val) =>
    let token = _dep_target(dep)
    _in_flight.unset(token)
    let temp = _temp_dir(token)
    let temp_path = FilePath(_auth, temp)

    let hash_bytes: Array[U8] val =
      try
        ContentHash(temp_path)?
      else
        _errors.push((dep.name, "content hash computation failed"))
        temp_path.remove()
        _launch_queued()
        _check_done()
        return
      end

    let computed_hex = Sha256.hex(hash_bytes)

    if dep.hash != "skip" then
      let config_hex = _config_hex(dep.hash)
      if computed_hex != config_hex then
        _errors.push((dep.name,
          "content hash mismatch: expected " + config_hex +
            " got " + computed_hex))
        temp_path.remove()
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
        _check_transitive(target)
        _launch_queued()
        _check_done()
        return
      end
    end

    if not temp_path.rename(target_path) then
      _errors.push(
        (dep.name, "failed to place dependency at " + target))
      temp_path.remove()
      _launch_queued()
      _check_done()
      return
    end

    _fetched = _fetched + 1
    _check_transitive(target)
    _launch_queued()
    _check_done()

  be _dep_failed(dep: DepEntry val, err: FetchError) =>
    let token = _dep_target(dep)
    _in_flight.unset(token)
    let temp_path = FilePath(_auth, _temp_dir(token))
    temp_path.remove()
    _errors.push((dep.name, err.message))
    _launch_queued()
    _check_done()

  fun ref _check_transitive(placed_dir: String) =>
    let config = Path.join(placed_dir, "pony.deps")
    if _processed_configs.contains(config) then return end
    _processed_configs.set(config)
    if FilePath(_auth, config).exists() then
      _pending_configs.set(config)
      ConfigAccess(_auth, config).read_config(
        _ResolveConfigNotify(this, config, false))
    end

  fun ref _check_done() =>
    if (_in_flight.size() == 0) and (_dep_queue.size() == 0) and
      (_pending_configs.size() == 0)
    then
      var errs: Array[(String val, String val)] iso =
        _errors = recover iso Array[(String val, String val)] end
      _notify.resolve_complete(consume errs, _fetched, _skipped)
    end

  fun _dep_target(dep: DepEntry val): String val =>
    if dep.hash == "skip" then
      dep.name + "@skip"
    else
      dep.name + "@" + _config_hex(dep.hash)
    end

  fun _temp_dir(target_name: String val): String val =>
    Path.join(_output_dir, "." + target_name + ".fetching")

  fun _config_hex(hash: String val): String val =>
    recover val
      String .> append(hash.substring(7)) .> lower_in_place()
    end

actor _ResolveConfigNotify is ConfigReadNotify
  """
  Routes config read results back to `ResolveDeps` with the originating
  config path and root/transitive distinction.
  """
  let _parent: ResolveDeps
  let _config_path: String
  let _is_root: Bool

  new create(parent: ResolveDeps, config_path: String, is_root: Bool) =>
    _parent = parent
    _config_path = config_path
    _is_root = is_root

  be config_loaded(config: ConfigFile val) =>
    _parent._config_loaded(_config_path, _is_root, config)

  be config_not_found() =>
    _parent._config_not_found(_config_path, _is_root)

  be config_error(message: String val) =>
    _parent._config_error(_config_path, _is_root, message)

actor _ResolveFetchNotify is FetchNotify
  """
  Routes fetch results back to `ResolveDeps` with the dependency entry.
  """
  let _parent: ResolveDeps
  let _dep: DepEntry val

  new create(parent: ResolveDeps, dep: DepEntry val) =>
    _parent = parent
    _dep = dep

  be fetch_failed(err: FetchError) =>
    _parent._dep_failed(_dep, err)

  be fetch_succeeded() =>
    _parent._dep_succeeded(_dep)
