use @labs[I64](x: I64)
interface G
  fun g(): I64
trait T
  fun f(): G val =>
    object val is G
      let v: I64 = @labs(-5)
      fun g(): I64 => v
    end
