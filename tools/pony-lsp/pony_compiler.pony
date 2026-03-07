use "pony_compiler"
use "term"
use "cli"
use "files"

use @get_compiler_exe_directory[Bool](
  output_path: Pointer[U8] tag,
  argv0: Pointer[U8] tag)
use @ponyint_pool_alloc_size[Pointer[U8] val](size: USize)
use @ponyint_pool_free_size[None](size: USize, p: Pointer[U8] tag)


actor PonyCompiler is LspCompiler
  """
  Actor wrapping the pony_compiler `Compiler` primitive to serialize compilation
  requests (libponyc is not fully thread-safe).

  Determines the ponyc standard library location automatically by finding the
  executable directory (pony-lsp is co-installed with ponyc) and adding
  `../packages` and `../../packages` relative to it — the same paths that
  ponyc itself uses.
  """
  let _pony_path: Array[String val] val
  let _installation_paths: Array[String val] val
  let _compilation_queue: Array[(FilePath, Array[String val] val, CompilerNotify tag)]

  var _defines: Array[String val] val
  var _pony_path_from_settings: Array[String val] val

  var _got_settings: Bool
    """
    The compiler will only start compiling after it got initialized
    by calling `apply_settings`.
    """
  var _run_id_gen: USize

  new create(pony_path': String) =>
    _pony_path = Path.split_list(pony_path')
    _installation_paths = _find_installation_paths()
    _compilation_queue = []

    _defines = _defines.create()
    _pony_path_from_settings = Array[String val].create()

    _got_settings = false

    // we start the runs with 1, as all other state-keeping things in this
    // program start with 0
    _run_id_gen = 1

  fun tag _find_installation_paths(): Array[String val] val =>
    """
    Find pony package paths relative to the running executable's directory.
    Since pony-lsp is installed alongside ponyc, the standard library lives
    at `../packages` (installed layout) or `../../packages` (source build
    layout) relative to the executable.
    """
    match \exhaustive\ _find_exe_directory()
    | let dir: String val =>
      recover val
        let paths = Array[String val](2)
        paths.push(recover val Path.join(dir, "../packages") end)
        paths.push(recover val Path.join(dir, "../../packages") end)
        paths
      end
    | None =>
      recover val Array[String val] end
    end

  fun tag _find_exe_directory(): (String val | None) =>
    """
    Find the directory containing the currently running executable using the
    same platform-specific mechanism as ponyc (readlink /proc/self/exe on
    Linux, _NSGetExecutablePath on macOS, etc.).
    """
    let buf_size: USize = 4096
    let buf = @ponyint_pool_alloc_size(buf_size)
    if @get_compiler_exe_directory(buf, "pony-lsp".cstring()) then
      let result = recover val String.copy_cstring(buf) end
      @ponyint_pool_free_size(buf_size, buf)
      result
    else
      @ponyint_pool_free_size(buf_size, buf)
      None
    end

  be apply_settings(settings: (Settings | None)) =>
    if not this._got_settings then
      this._got_settings = true
      // trigger all queued compilations
      try
        while this._compilation_queue.size() > 0 do
          (let package, let paths, let notify) = this._compilation_queue.pop()?
          this.compile(package, paths, notify)
        end
      end
    end

    match settings
    | let settings': Settings =>
      _pony_path_from_settings = settings'.ponypath()
      _defines = settings'.defines()
    end

  be compile(package: FilePath, paths: Array[String val] val, notify: CompilerNotify tag) =>
    if not this._got_settings then
      // enqueue compilation
      this._compilation_queue.push((package, paths, notify))
    end
    // Search order: installation paths first (prevents PONYPATH from
    // overriding builtin, per ponylang/ponyc#3779), then PONYPATH, then
    // workspace-specific paths (corral dependencies).
    let package_paths: Array[String val] val =
      recover val
        let tmp = _installation_paths.clone()
        for p in _pony_path.values() do
          tmp.push(p)
        end
        tmp.append(_pony_path_from_settings)
        tmp.append(paths)
        tmp
      end
    let result = Compiler.compile(
      package,
      package_paths
      where
        user_flags = this._defines,
        release = false,
        verbosity = VerbosityQuiet,
        limit = PassFinaliser
    )
    let run_id = _run_id_gen = _run_id_gen + 1
    notify.done_compiling(package, result, run_id)

trait tag LspCompiler
  be apply_settings(settings: (Settings|None))
    """
    Provide settings to initialize or reconfigure the compiler.

    `None` can be provided when no new settings should be applied, but the
    initialization step should be completed. E.g. when no settings are available.
    """
  be compile(package: FilePath, paths: Array[String val] val, notify: CompilerNotify tag)

interface CompilerNotify
  """
  Notify which is called by the compiler when it is done.
  """
  be done_compiling(package: FilePath, result: (Program val | Array[Error val] val), run: USize)
