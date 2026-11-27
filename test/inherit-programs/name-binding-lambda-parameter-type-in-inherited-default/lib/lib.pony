trait Foo
  fun make(): String =>
    let f = {(h: Helper): String => h.name()}
    f(Helper)

primitive Helper
  fun name(): String => "lib.Helper"
