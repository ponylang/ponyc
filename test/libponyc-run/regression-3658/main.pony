primitive Foo[T: (Unsigned & UnsignedInteger[T])]
  fun boom() =>
    iftype T <: U8 then
      None
    end
  // more cases
  fun baam1[T: (Unsigned & UnsignedInteger[T] & U8)](t: T): U32 =>
    t.u32()
  fun baam2[T: (U8 & Unsigned & UnsignedInteger[T])](t: T): U32 =>
    t.u32()

actor Main
  new create(env: Env) =>
    Foo[U8].boom()
