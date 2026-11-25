trait T
  fun f[Foo: Stringable val](x: Foo): String =>
    let t: T box = this
    t.name() + x.string()

  fun g[Foo: Stringable val](x: Foo): String =>
    let l = {(): String => x.string() }
    l()

  fun name(): String => "t"
