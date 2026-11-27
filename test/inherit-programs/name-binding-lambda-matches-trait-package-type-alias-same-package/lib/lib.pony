type Num is (U8 | U16)

trait Foo
  fun make(): String =>
    let n: Num = U8(1)
    let f = {(): String => match n | let x: U8 => "u8" else "u16" end}
    f()

class Inside is Foo
