trait T
  fun mk(): Stringable val => object val is Stringable fun string(): String iso^ => __loc.type_name().clone() end
