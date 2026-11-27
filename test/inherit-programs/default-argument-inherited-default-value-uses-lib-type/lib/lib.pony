trait Foo
  fun make(h: Helper = Helper): String => h.name()

primitive Helper
  fun name(): String => "lib.Helper"
