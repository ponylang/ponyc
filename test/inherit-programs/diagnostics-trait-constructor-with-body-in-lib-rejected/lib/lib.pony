trait Foo
  new create() =>
    _Helper.name()
  fun make(): String => _Helper.name()

primitive _Helper
  fun name(): String => "lib._Helper"
