use imported = "collections"

trait Group[A: Any val]
  fun check(): U64 => 10
  fun check(): U64 iftype A <: U8 =>
    let size = {(): USize => imported.List[U8].size() }
    size().u64() + 20

  fun check(): U64 iftype A <: (U8 | U16) => 21
