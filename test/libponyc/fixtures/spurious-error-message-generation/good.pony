// Syntactically valid file in the same package as bad.pony.
// The loop variable 'i' is reused across consecutive for loops; before the
// fix for issue #4160, a parse error in bad.pony caused spurious
// "can't reuse name 'i'" errors here.
class Good
  new create() =>
    let a: Array[U8] = Array[U8]
    for i in a.values() do
      None
    end
    for i in a.values() do
      None
    end
    for i in a.values() do
      None
    end
