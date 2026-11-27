trait Foo
  fun make(): String =>
    let f = {(): String => Helper.name()}
    f()

primitive Helper
  fun name(): String => "lib.Helper"
