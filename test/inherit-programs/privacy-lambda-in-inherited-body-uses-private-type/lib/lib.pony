trait Foo
  fun make(): String =>
    let f = {(): String => _Helper.name()}
    f()

primitive _Helper
  fun name(): String => "lib._Helper"
