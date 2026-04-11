use "cli"
use "files"
use "collections"
use "term"

use "pony_compiler"

use @get_compiler_exe_directory[Bool](
  output_path: Pointer[U8] tag,
  argv0: Pointer[U8] tag)
use @ponyint_pool_alloc_size[Pointer[U8] val](size: USize)
use @ponyint_pool_free_size[None](size: USize, p: Pointer[U8] tag)

actor PonyCompiler is LspCompiler
  """
  Actor wrapping the pony_compiler `CompileSession`
  to serialize compilation requests.

  Compilation will notify the caller for each requested pass.

  Determines the ponyc standard library location
  automatically by finding the executable directory
  (pony-lsp is co-installed with ponyc) and adding
  `../packages` and `../../packages` relative to it
  -- the same paths that ponyc itself uses.
  """
  let _pony_path: Array[String val] val
  let _installation_paths: Array[String val] val
  let _compilation_queue:
    Array[(FilePath, Array[String val] val, CompilerNotify tag)]
  var _defines: Array[String val] val
  var _pony_path_from_settings: Array[String val] val
  var _got_settings: Bool
    """
    The compiler will only start compiling after it
    got initialized by calling `apply_settings`.
    """
  var _run_id_gen: USize

  new create(pony_path': String, argv0: String val = "") =>
    _pony_path = Path.split_list(pony_path')
    _installation_paths = _find_installation_paths(argv0)
    _compilation_queue = []

    _defines = _defines.create()
    _pony_path_from_settings = Array[String val].create()

    _got_settings = false

    // we start the runs with 1, as all other
    // state-keeping things in this program start
    // with 0
    _run_id_gen = 1

  fun tag _find_installation_paths(argv0: String val): Array[String val] val =>
    """
    Find pony package paths relative to the running executable's directory.
    Since pony-lsp is installed alongside ponyc, the standard library lives at
    `../packages` (installed layout) or `../../packages` (source build layout)
    relative to the executable.
    """
    match \exhaustive\ _find_exe_directory(argv0)
    | let dir: String val =>
      recover val
        Array[String val](2)
          .> push(
            recover val
              Path.join(dir, "../packages")
            end)
          .> push(
            recover val
              Path.join(dir, "../../packages")
            end)
      end
    | None =>
      recover val Array[String val] end
    end

  fun tag _find_exe_directory(argv0: String val): (String val | None) =>
    """
    Find the directory containing the currently running executable
    using the same platform-specific mechanism as ponyc.
    """
    let buf_size: USize = 4096
    let buf = @ponyint_pool_alloc_size(buf_size)
    if @get_compiler_exe_directory(buf, argv0.cstring()) then
      let result =
        recover
          val String.copy_cstring(buf)
        end
      @ponyint_pool_free_size(buf_size, buf)
      result
    else
      @ponyint_pool_free_size(buf_size, buf)
      None
    end

  be apply_settings(settings: (Settings | None)) =>
    match settings
    | let settings': Settings =>
      _pony_path_from_settings = settings'.ponypath()
      _defines = settings'.defines()
    end

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

  be compile(
    package: FilePath,
    paths: Array[String val] val,
    notify: CompilerNotify tag,
    notify_passes: Array[PassId] val = [PassFinaliser])
  =>
    """
    Compile a package with the given paths.

    Notify `notify` for every pass provided in `notify_passes`. The default is
    to only notify once for the result of the finaliser pass.

    If an error happens while compiling up to the next pass, the `notify` will
    be notified about the errors and no more notifications will arrive.

    Compilation is done when either the `result` param of
    `CompilerNotify.on_compile_result()` is an Array of errors or if the `pass`
    is the last one in the `notify_passes`.

    If `notify_passes` is empty, `notify` will never be called.
    """
    if not this._got_settings then
      // enqueue compilation and dont execute it yet
      this._compilation_queue.push((package, paths, notify))
      return
    end
    // Search order: installation paths first (prevents PONYPATH from overriding
    // builtin, per ponylang/ponyc#3779), then PONYPATH, then workspace-specific
    // paths (corral dependencies).
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
    let sorted_passes = Sort[Array[PassId], PassId](notify_passes.clone())
    try
      let first_pass = sorted_passes.shift()?
      let run_id = _run_id_gen = _run_id_gen + 1
      let session =
        CompileSession.create(
          package,
          package_paths
          where
            user_flags = this._defines,
            release = false,
            verbosity = VerbosityQuiet,
            limit = first_pass)
      try
        let program = session.program() as Program val
        notify.on_compile_result(package, run_id, first_pass, program)
        for pass in sorted_passes.values() do
          if session.continue_to(pass) then
            // compilation success up until pass
            let result =
              try
                session.program() as Program val
              else
                session.errors()
              end
            notify.on_compile_result(package, run_id, pass, result)
          else
            // compile error
            notify.on_compile_result(package, run_id, pass, session.errors())
            // don't continue upon errors
            break
          end
        end
      else
        // compile error up until first pass
        notify.on_compile_result(package, run_id, first_pass, session.errors())
      end
      session.dispose()
    end

trait tag LspCompiler
  """
  Interface for triggering compilation.
  """

  be apply_settings(settings: (Settings | None))
    """
    Provide settings to initialize or reconfigure the compiler.

    `None` can be provided when no new settings should be applied,
    but the initialization step should be completed.
    """

  be compile(
    package: FilePath,
    paths: Array[String val] val,
    notify: CompilerNotify tag,
    notify_passes: Array[PassId] val)
    """
    Compile the given package and call the `notify` with the compilation result
    after the provided passes in `notify_passes`.
    """

interface CompilerNotify
  """
  Notify which is called by the compiler when it is done with a requested pass.
  """

  be on_compile_result(
    package: FilePath,
    compile_run: USize,
    pass: PassId,
    result: (Program val | Array[Error val] val)
    )
    """
    Called when a compilation result after the provided pass is available.

    Do not keep the Program or any part of it around without copying,
    if further passes are executed.
    The underlying AST might be modified and parts of it replaced and deleted.
    """
