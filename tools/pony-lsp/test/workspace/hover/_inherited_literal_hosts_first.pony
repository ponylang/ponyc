class _HoverFirstHostU8 is _HoverHostedAfter
  fun v(): U8 => 1

class _HoverFirstHostString is _HoverHostedAfter
  fun v(): String => "s"

trait _HoverHostedAfter
  """
  As `_HoverHosted`, written after the classes that inherit it.
  """
  fun v(): (U8 | String)

  fun mk(): _HoverRunner =>
    let x = v()
    object val is _HoverRunner
      fun apply(): String => x.string()
    end
