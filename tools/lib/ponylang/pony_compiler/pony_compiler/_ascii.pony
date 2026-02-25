primitive _ASCII

  fun tag is_binary(c: U8): Bool =>
    (c == '0') or (c == '1')

  fun tag is_decimal(c: U8): Bool =>
    _is_in_range(c, '0', '9')

  fun tag _is_in_range(c: U8, lower_incl: U8, upper_incl: U8): Bool =>
    (lower_incl <= c) and (c <= upper_incl)

  fun tag is_hexadecimal(c: U8): Bool =>
    is_decimal(c) or _is_in_range(c, 'a', 'f') or _is_in_range(c, 'A', 'F')
