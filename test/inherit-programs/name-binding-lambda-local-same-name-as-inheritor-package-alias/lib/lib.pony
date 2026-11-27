trait Foo
  fun f(): String =>
    let g = {(): String => let c: U8 = 3; c.string()}
    g()
