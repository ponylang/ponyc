use "cdefine:SHIM_BASE=30"
use "cincludedir:./include"

use @shim_answer[I32]()

actor Main
  new create(env: Env) =>
    env.exitcode(@shim_answer())
