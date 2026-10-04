use @pony_exitcode[None](code: I32)

class Converter
  fun apply(s: String): USize => s.size()
  fun apply(n: USize): String => n.string()
  fun apply(b: Bool): String => if b then "true" else "false" end

actor Main
  new create(env: Env) =>
    let c = Converter
    let r1 = c("hello")
    let r2 = c(USize(42))
    let r3 = c(true)

    if (r1 == 5) and (r2 == "42") and (r3 == "true") then
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
