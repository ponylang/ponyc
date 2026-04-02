use "path:../lib/ponylang/pony_compiler/"
use "files"
use "pony_compiler"

actor Main
  new create(env: Env) =>
    let channel = Stdio(env.out, env.input)
    let pony_path =
      match \exhaustive\ PonyPath(env)
      | let p: String => p
      | None => ""
      end
    let argv0 =
      try env.args(0)? else "" end
    let language_server =
      LanguageServer(
        channel,
        env,
        PonyCompiler(pony_path, argv0))
    // at this point the server should listen to
    // incoming messages via stdin
