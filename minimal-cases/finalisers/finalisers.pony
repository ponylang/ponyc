use "process"
use "files"
use "collections"
use "buffered"

primitive PrimitiveInitFinal
  fun _init() =>
    @printf[I32]("primitive init\n".cstring())

  fun _final() =>
    @printf[I32]("primitive final\n".cstring())

class EmbedFinal
  fun _final() =>
    @printf[I32]("embed final\n".cstring())

class ClassFinal
  embed f: EmbedFinal = EmbedFinal

  fun _final() =>
    @printf[I32]("class final\n".cstring())

actor ActorFinal
  new create(env: Env) =>
    ClassFinal

  fun _final() =>
    @printf[I32]("actor final\n".cstring())



actor Main
  new create(env: Env) =>
    if env.args.size() > 1
    then
      // we're the child process
      ActorFinal(env)
    else
      let expected = recover val
        ["primitive init"; "primitive final"; "actor final"]
      end
      ProcessOutput(expected).run(env)
    end

actor ProcessOutput
  let _expected : Array[String] val

  new create(expectedResults: Array[String] val) =>
    _expected = expectedResults

  be run(env: Env) =>
    let client = ProcessClient(env, _expected)
    let processFilename: String = try env.args(0) else "" end
    // define the binary to run
    try
      let path = FilePath(env.root as AmbientAuth, 
                          processFilename)

      // define the arguments; first arg is always the binary name
      let args: Array[String] iso = recover [processFilename; "dummy to avoid reforking"] end

      // create a ProcessMonitor and spawn the child process
      let auth = env.root as AmbientAuth
      let pm: ProcessMonitor = ProcessMonitor(auth, 
        consume client, path,
        consume args, recover val Array[String]() end)
    else
      env.out.print("Could not create FilePath!")
    end

// define a client that implements the ProcessNotify interface
class ProcessClient is ProcessNotify
  let _env: Env
  let _expected: Array[String] val
  let _actual: Reader = Reader

  new iso create(env: Env, expected: Array[String] val) =>
    _env = env
    _expected = expected

  fun ref stdout(process: ProcessMonitor ref, data: Array[U8] iso) =>
    _actual.append(consume data)
    // let out = String.from_array(consume data)
    // _env.out.print("STDOUT: " + out)

  fun ref stderr(process: ProcessMonitor ref, data: Array[U8] iso) =>
    let err = String.from_array(consume data)
    _env.out.print("STDERR: " + err)

  fun ref failed(process: ProcessMonitor ref, err: ProcessError) =>
    match err
    | ExecveError => _env.out.print("ProcessError: ExecveError")
    | PipeError => _env.out.print("ProcessError: PipeError")
    | ForkError => _env.out.print("ProcessError: ForkError")
    | WaitpidError => _env.out.print("ProcessError: WaitpidError")
    | WriteError => _env.out.print("ProcessError: WriteError")
    | KillError => _env.out.print("ProcessError: KillError")
    | CapError => _env.out.print("ProcessError: CapError")
    | Unsupported => _env.out.print("ProcessError: Unsupported")
    else _env.out.print("Unknown ProcessError!")
    end

  fun ref dispose(process: ProcessMonitor ref, child_exit_code: I32) =>
    if child_exit_code != 0
    then
      _env.out.print("Child exit code: " + child_exit_code.string())
      @_exit[None](I32(child_exit_code))
      return
    end

    let expectedS = Set[String]
    for s in _expected.values() do
      expectedS.set(s)
    end

    let actualS = Set[String]
    while true do
      actualS.set(try _actual.line() else break end)
    end

    if expectedS != actualS
    then
      @_exit[None](I32(1))
    else
      @_exit[None](I32(0))
    end
