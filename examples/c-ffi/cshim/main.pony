use "cdefine:CSHIM_ANSWER=42"
use "cincludedir:./include"

use @cshim_answer[I32]()
use @cshim_version[I32]()

actor Main
  new create(env: Env) =>
    env.out.print("answer from the shim: " + @cshim_answer().string())
    env.out.print("version from the header: " + @cshim_version().string())
