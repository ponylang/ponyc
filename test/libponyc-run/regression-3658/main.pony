primitive Foo[T: (Unsigned & UnsignedInteger[T])]
  fun boom() =>
    iftype T <: U8 then
      None
    end
  // more cases
  fun baam1[S: (Unsigned & UnsignedInteger[S] & U8)](t: S): U32 =>
    t.u32()
  fun baam2[S: (U8 & Unsigned & UnsignedInteger[S])](t: S): U32 =>
    t.u32()

actor Main
  new create(env: Env) =>
    Foo[U8].boom()
