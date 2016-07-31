interface ANSINotify
  """
  Receive input from an ANSITerm.
  """
  fun ref apply(term: ANSITerm ref, input: U8) =>
    None

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

  fun ref prompt(term: ANSITerm ref, value: String) =>
    None

  fun ref size(rows: U16, cols: U16) =>
    None

  fun ref closed() =>
    None
