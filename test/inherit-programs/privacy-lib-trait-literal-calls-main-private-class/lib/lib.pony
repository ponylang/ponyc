interface val Show
  fun show(): String
trait Named
  fun name(): String
  fun val m(): Show =>
    let me = this
    object val is Show fun show(): String => "show " + me.name() end
