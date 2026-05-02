class _TCallLambda
  fun f_lambda(a: _TCallA): None =>
    a.f_a()
    let _ =
      object
        fun g(): None =>
          None
      end
    a.f_a()
    None
