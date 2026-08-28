// from issue #568
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let lit = [({(s: String): String => s + "1" }, {(s: String): String => s + "2" })
               ({(s: String): String => s + "3" }, {(s: String): String => s + "4" })
               ({(s: String): String => s + "5" }, {(s: String): String => s + "6" })]

    var result = String()
    for (la, lb) in lit.values() do
      result = result + la("a") + lb("b")
    end

    if result == "a1b2a3b4a5b6" then
      @pony_exitcode(1)
    end
