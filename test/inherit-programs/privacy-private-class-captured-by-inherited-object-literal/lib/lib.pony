trait Named
  fun name(): String
  fun val m(): Stringable val =>
    let me = this
    object val is Stringable fun string(): String iso^ => ("show " + me.name()).clone() end
