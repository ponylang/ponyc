use "ast"
use "term"
use "cli"
use "files"

use @get_compiler_exe_directory[Bool](
  output_path: Pointer[U8] tag,
  argv0: Pointer[U8] tag)
use @ponyint_pool_alloc_size[Pointer[U8] val](size: USize)
use @ponyint_pool_free_size[None](size: USize, p: Pointer[U8] tag)

actor PonyCompiler
  """
  Actor wrapping the pony-ast `Compiler` primitive to serialize compilation
  requests (libponyc is not fully thread-safe).

  Determines the ponyc standard library location automatically by finding the
  executable directory (pony-lsp is co-installed with ponyc) and adding
  `../packages` and `../../packages` relative to it — the same paths that
  ponyc itself uses.
  """
  let _pony_path: Array[String val] val
  let _installation_paths: Array[String val] val

  var _run_id_gen: USize

  new create(pony_path': String) =>
    _pony_path = Path.split_list(pony_path')
    _installation_paths = _find_installation_paths()
    _run_id_gen = 0

  fun tag _find_installation_paths(): Array[String val] val =>
    """
    Find pony package paths relative to the running executable's directory.
    Since pony-lsp is installed alongside ponyc, the standard library lives
    at `../packages` (installed layout) or `../../packages` (source build
    layout) relative to the executable.
    """
    match _find_exe_directory()
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

  be compile(package: FilePath, paths: Array[String val] val, notify: CompilerNotify tag) =>
    // Search order: installation paths first (prevents PONYPATH from
    // overriding builtin, per ponylang/ponyc#3779), then PONYPATH, then
    // workspace-specific paths (corral dependencies).
    let package_paths: Array[String val] val =
      recover val
        let tmp = _installation_paths.clone()
        for p in _pony_path.values() do
          tmp.push(p)
        end
        for p in paths.values() do
          tmp.push(p)
        end
        tmp
      end
    let result = Compiler.compile(package, package_paths)
    let run_id = _run_id_gen = _run_id_gen + 1
    notify.done_compiling(package, result, run_id)


interface CompilerNotify
  """
  Notify which is called by the compiler when it is done.
  """
  be done_compiling(package: FilePath, result: (Program val | Array[Error val] val), run: USize)


/*
class CompilerState
  """
  State that is needed to fulfill
  LSP queries
  """
  let package: (Package | None)

  new create(package': Package) =>
    package = package'

  new empty() =>
    package = None


actor PonyCompiler
  let env: Env
  let channel: Stdio
  var state: (CompilerState | None) = None

  new create(env': Env, channel': Stdio) =>
    env = env'
    channel = channel'

  be apply(uri: String, notifier: CompilerNotifier tag) =>
    Log(channel, "PonyCompiler apply")
    // extract PONYPATH
    var pony_path = 
      match PonyPath(env)
      | let pp: String => pp
      else
        Log(channel, "Couldn't retrieve PONYPATH")
        return
      end
    var corral_path: (String | None) =
      if FilePath(FileAuth(env.root), Path.dir(uri) + "/../_corral").exists() then
        Path.dir(uri) + "/../_corral"
      elseif FilePath(FileAuth(env.root), Path.dir(uri) + "/_corral").exists() then
        Path.dir(uri) + "/_corral"
      else
        None
      end

    match corral_path
    | None | "" => None
    | let corral_path': String val =>
      var paths = [pony_path]
      let handler =
        {ref(dir_path: FilePath val, dir_entries: Array[String val] ref)(paths): None =>
          Log(channel, "## folder " + dir_path.path) 
          if Path.base(dir_path.path) != "_corral" then
            dir_entries.clear()
            paths.push(dir_path.path)
          end}
      FilePath(FileAuth(env.root), corral_path').walk(handler, false)
      pony_path = ":".join(paths.values())
    end
    match Compiler.compile(FilePath(FileAuth(env.root), Path.dir(uri)), pony_path)
    | let p: Program =>
      try
        Log(channel, "Program received")
        state = CompilerState(p.package() as Package)
      else
        Log(channel, "Error getting package from program")
      end
    | let errs: Array[Error] val =>
      Log(channel, "Compile errors: " + errs.size().string())
      for err in errs.values() do
        Log(channel, "Error: " + err.msg)
        notifier.on_error(err.file, err.line, err.pos, err.msg)
      end
    end
    Log(channel, "Stdlib path: " + pony_path)
    Log(channel, "PonyCompiler called CompilerNotifier done()")
    notifier.done()

  be get_type_at(file_path: String, line: USize, position: USize, notifier: TypeNotifier tag) =>
    // TODO: fix
    notifier.type_notified(None)
*/
