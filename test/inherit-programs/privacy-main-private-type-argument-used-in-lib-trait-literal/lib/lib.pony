interface HasName
  fun name(): String
trait Show
  fun show(): String
trait Foo[A: HasName val]
  fun g(x: A): Show val => object val is Show fun show(): String => x.name() end
