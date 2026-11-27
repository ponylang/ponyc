trait Foo
  fun _secret(): String => "secret"
  fun d(): String => _secret()
