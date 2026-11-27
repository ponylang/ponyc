trait T1
  fun f(): String =>
    let g = {(): String => let u: U8 = 3; u.string()}
    g()
