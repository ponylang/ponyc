interface val _HoverRunner
  """
  Runs to a String.
  """
  fun apply(): String

trait _HoverHosted
  """
  Has a method with an object literal that two classes inherit, each with its
  own `v`.
  Hovering on `x` inside the literal shows the trait's own type for it.
  """
  fun v(): (U8 | String)

  fun mk(): _HoverRunner =>
    let x = v()
    object val is _HoverRunner
      fun apply(): String => x.string()
    end

class _HoverHostU8 is _HoverHosted
  fun v(): U8 => 1

class _HoverHostString is _HoverHosted
  fun v(): String => "s"
