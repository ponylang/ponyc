primitive _H
  fun name(): String => "lib helper"
trait Foo
  fun describe(): String => _H.name()
  fun mk(): _H => _H
  fun d2(): String => mk().name()
