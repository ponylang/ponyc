trait Foo
  fun make(): String => Helper.name()

primitive Helper
  fun name(): String => "lib.Helper"
