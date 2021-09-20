"""
From #2245
"""

use "serialise"
use @pony_exitcode[None](code: I32)

class V
  let _v: String = ""

actor Main
  new create(env: Env) =>
    try
      let auth = env.root as AmbientAuth
      let v: V = V
      Serialised(SerialiseAuth(auth), v)?
      @pony_exitcode(1)
    end
