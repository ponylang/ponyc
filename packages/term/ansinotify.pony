interface ANSINotify
  """
  Receive input from an ANSITerm.
  """
  fun ref apply(input: U8): Bool =>
    true

  fun ref up(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref down(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref left(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref right(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref delete(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref insert(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref home(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref end_key(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref page_up(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref page_down(ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref fn_key(i: U8, ctrl: Bool, alt: Bool, shift: Bool) =>
    None

  fun ref closed() =>
    None
