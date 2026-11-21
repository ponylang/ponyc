class _BigUint
  """
  Unsigned big integer. Little-endian U64 limbs with no high zero limbs;
  the empty array is zero. Most operations mutate in place; use clone for
  a copy.
  """
  var _limbs: Array[U64]

  new create() =>
    _limbs = Array[U64]

  new from_u64(x: U64) =>
    _limbs = Array[U64]
    if x != 0 then
      _limbs.push(x)
    end

  fun is_zero(): Bool =>
    _limbs.size() == 0

  fun ref normalize() =>
    try
      while (_limbs.size() > 0) and (_limbs(_limbs.size() - 1)? == 0) do
        _limbs.pop()?
      end
    end

  fun ref mul_add(m: U64, a: U64) =>
    """
    Sets this to this * m + a.
    """
    var carry: U128 = a.u128()
    try
      var i: USize = 0
      while i < _limbs.size() do
        let t = (_limbs(i)?.u128() * m.u128()) + carry
        _limbs(i)? = (t and 0xFFFF_FFFF_FFFF_FFFF).u64()
        carry = t >> 64
        i = i + 1
      end
    end
    if carry != 0 then
      _limbs.push(carry.u64())
    end

  fun ref shl_in_place(n: USize) =>
    """
    Shift left by n bits.
    """
    if is_zero() or (n == 0) then
      return
    end
    let limb_shift = n / 64
    let bit_shift = n % 64
    try
      if limb_shift > 0 then
        var n0: USize = 0
        while n0 < limb_shift do
          _limbs.push(0)
          n0 = n0 + 1
        end
        var i = _limbs.size()
        while i > limb_shift do
          i = i - 1
          _limbs(i)? = _limbs(i - limb_shift)?
        end
        var j: USize = 0
        while j < limb_shift do
          _limbs(j)? = 0
          j = j + 1
        end
      end
      if bit_shift > 0 then
        var carry: U64 = 0
        var i = limb_shift
        while i < _limbs.size() do
          let v = _limbs(i)?
          _limbs(i)? = (v << bit_shift.u64()) or carry
          carry = v >> (64 - bit_shift).u64()
          i = i + 1
        end
        if carry != 0 then
          _limbs.push(carry)
        end
      end
    end

  fun bit_length(): USize =>
    if _limbs.size() == 0 then
      0
    else
      try
        let top = _limbs(_limbs.size() - 1)?
        ((_limbs.size() - 1) * 64) + (64 - top.clz().usize())
      else
        0
      end
    end

  fun bit(i: USize): Bool =>
    try
      ((_limbs(i / 64)? >> (i % 64).u64()) and 1) == 1
    else
      false
    end

  fun cmp(o: _BigUint box): I32 =>
    """
    -1 if this < o, 0 if equal, 1 if this > o.
    """
    if _limbs.size() < o._limbs.size() then
      return -1
    end
    if _limbs.size() > o._limbs.size() then
      return 1
    end
    try
      var i = _limbs.size()
      while i > 0 do
        i = i - 1
        let a = _limbs(i)?
        let b = o._limbs(i)?
        if a < b then
          return -1
        end
        if a > b then
          return 1
        end
      end
    end
    0

  fun ref sub_in_place(o: _BigUint box) =>
    """
    this = this - o. Requires this >= o.
    """
    try
      var borrow = false
      var i: USize = 0
      while i < o._limbs.size() do
        let a = _limbs(i)?.u128()
        let b = o._limbs(i)?.u128() + (if borrow then 1 else 0 end)
        if a >= b then
          _limbs(i)? = (a - b).u64()
          borrow = false
        else
          _limbs(i)? = ((a + (U128(1) << 64)) - b).u64()
          borrow = true
        end
        i = i + 1
      end
      if borrow then
        var k = o._limbs.size()
        while borrow do
          let a = _limbs(k)?
          if a == 0 then
            _limbs(k)? = 0xFFFF_FFFF_FFFF_FFFF
          else
            _limbs(k)? = a - 1
            borrow = false
          end
          k = k + 1
        end
      end
    end
    normalize()

  fun clone(): _BigUint =>
    let r = _BigUint
    for v in _limbs.values() do
      r._limbs.push(v)
    end
    r

primitive _StrToFloat
  """
  Parses a float from the bytes [index, size) of s, using the C strtod
  syntax in the C locale: optional leading whitespace and sign, decimal or
  hexadecimal floating point, or Inf/Infinity/NaN (case insensitive, NaN
  with an optional parenthesized payload). Every byte must be consumed.
  The result is the nearest representable value, ties to even. Errors on
  overflow to infinity; underflow returns the nearest subnormal or zero.
  A NaN payload is ignored: the result is the canonical quiet NaN with the
  requested sign.
  """

  fun f32(s: Pointer[U8] box, size: USize, index: USize): F32 ? =>
    F32.from_bits(_parse(s, size, index, 24, 8, 127)?.u32())

  fun f64(s: Pointer[U8] box, size: USize, index: USize): F64 ? =>
    F64.from_bits(_parse(s, size, index, 53, 11, 1023)?)

  fun _exponent_sat(): I64 =>
    """
    Explicit exponents saturate at this value. It is far beyond any
    exponent that affects the result: the range checks in _round_to_float
    settle those as overflow errors or zeros. It is also far beyond the
    digit counts an in-memory string can contribute, so the compensation
    for fractional and dropped digits in _parse can never cancel the
    saturation, and the combined exponent cannot overflow I64.
    """
    100_000_000_000_000_000

  fun _dec_digit_cap(): USize =>
    """
    Significant decimal digits kept in m; further digits only set sticky
    and count as dropped. 800 covers the longest exact halfway expansion
    an f64 rounding comparison can need (767 significant digits) with
    margin. Couples with the decimal bounds in _round_to_float, which
    assume m has at most this many digits.
    """
    800

  fun _hex_digit_cap(): USize =>
    """
    Significant hex digits kept in m; further digits only set sticky and
    count as dropped. 300 hex digits is 1200 bits, far past the 53 or 24
    significand bits plus the alignment a bounded hex exponent can need.
    Couples with the hex magnitude pre-checks in _round_to_float.
    """
    300

  fun _is_space(c: U8): Bool =>
    (c == 0x20) or ((c >= 0x09) and (c <= 0x0D))

  fun _is_digit(c: U8): Bool =>
    (c >= '0') and (c <= '9')

  fun _is_hex(c: U8): Bool =>
    _is_digit(c) or
      ((c >= 'a') and (c <= 'f')) or
      ((c >= 'A') and (c <= 'F'))

  fun _hex_val(c: U8): U64 =>
    if _is_digit(c) then
      (c - '0').u64()
    elseif (c >= 'a') and (c <= 'f') then
      ((c - 'a') + 10).u64()
    else
      ((c - 'A') + 10).u64()
    end

  fun _lower(c: U8): U8 =>
    if (c >= 'A') and (c <= 'Z') then c + 0x20 else c end

  fun _match_word(s: Pointer[U8] box, size: USize, i: USize, w: String)
    : USize
  =>
    """
    Case-insensitively match w at byte position i; return the number of
    bytes matched, possibly short of w.size().
    """
    var n: USize = 0
    try
      while (n < w.size()) and ((i + n) < size) do
        if _lower(s._apply(i + n)) != _lower(w(n)?) then
          break
        end
        n = n + 1
      end
    end
    n

  fun _read_exponent(s: Pointer[U8] box, size: USize, letter: USize)
    : (I64, USize)
  =>
    """
    Read a signed decimal exponent whose marker letter ('e', 'E', 'p' or
    'P') is at byte position letter. Returns the value saturated at
    +/-_exponent_sat() and the position just past the exponent. If no
    digit follows the marker and optional sign, returns (0, letter),
    leaving the marker unconsumed.
    """
    var i = letter + 1
    var neg = false
    if i < size then
      let c = s._apply(i)
      if (c == '-') or (c == '+') then
        neg = c == '-'
        i = i + 1
      end
    end
    var v: I64 = 0
    var any = false
    while i < size do
      let c = s._apply(i)
      if _is_digit(c) then
        any = true
        if v < _exponent_sat() then
          v = ((v * 10) + (c - '0').i64()).min(_exponent_sat())
        end
        i = i + 1
      else
        break
      end
    end
    if any then
      ((if neg then -v else v end), i)
    else
      (0, letter)
    end

  fun _parse(
    s: Pointer[U8] box,
    size: USize,
    index: USize,
    p: USize,
    exp_bits: USize,
    bias: I32)
    : U64 ?
  =>
    let sign_shift = (p - 1) + exp_bits
    let sign_bit = U64(1) << sign_shift.u64()
    let exp_all = ((U64(1) << exp_bits.u64()) - 1) << (p - 1).u64()
    var i = index

    while (i < size) and _is_space(s._apply(i)) do
      i = i + 1
    end

    var neg = false
    if i < size then
      let c = s._apply(i)
      if (c == '-') or (c == '+') then
        neg = c == '-'
        i = i + 1
      end
    end

    if i >= size then
      error
    end

    let l0 = _lower(s._apply(i))

    if l0 == 'i' then
      if _match_word(s, size, i, "inf") < 3 then
        error
      end
      i = i + 3
      if _match_word(s, size, i, "inity") == 5 then
        i = i + 5
      end
      if i != size then
        error
      end
      return (if neg then sign_bit else 0 end) or exp_all
    end

    if l0 == 'n' then
      if _match_word(s, size, i, "nan") < 3 then
        error
      end
      i = i + 3
      if (i < size) and (s._apply(i) == '(') then
        var j = i + 1
        while j < size do
          let cj = s._apply(j)
          if _is_digit(cj) or
            ((cj >= 'a') and (cj <= 'z')) or
            ((cj >= 'A') and (cj <= 'Z')) or
            (cj == '_')
          then
            j = j + 1
          else
            break
          end
        end
        if (j < size) and (s._apply(j) == ')') then
          i = j + 1
        end
      end
      if i != size then
        error
      end
      let quiet_bit = U64(1) << (p - 2).u64()
      return (if neg then sign_bit else 0 end) or exp_all or quiet_bit
    end

    // Numeric input: decimal, or hexadecimal after a 0x/0X marker.
    // Collect significant digits into m so that
    //   decimal: value = m * 10^exp10
    //   hex:     value = m * 2^exp2
    // Digits past _dec_digit_cap()/_hex_digit_cap() are dropped: nonzero
    // ones set sticky, and each one is compensated in the exponent, so
    // the invariant holds for any input length.
    let m = _BigUint
    var ndigits: USize = 0
    var dropped: USize = 0
    var sticky = false
    var frac: USize = 0
    var exp10: I64 = 0
    var exp2: I64 = 0
    var any = false

    var is_hex = false
    if (l0 == '0') and ((i + 1) < size) then
      let c1 = s._apply(i + 1)
      is_hex = (c1 == 'x') or (c1 == 'X')
    end

    if is_hex then
      i = i + 2
      var seen_point = false
      while i < size do
        let c = s._apply(i)
        if _is_hex(c) then
          any = true
          let d = _hex_val(c)
          if seen_point then
            frac = frac + 1
          end
          if (d != 0) or (ndigits > 0) then
            if ndigits < _hex_digit_cap() then
              m.mul_add(16, d)
              ndigits = ndigits + 1
            else
              if d != 0 then
                sticky = true
              end
              dropped = dropped + 1
            end
          end
          i = i + 1
        elseif (c == '.') and not seen_point then
          seen_point = true
          i = i + 1
        else
          break
        end
      end
      if not any then
        error
      end
      if i < size then
        let c = s._apply(i)
        if (c == 'p') or (c == 'P') then
          (let pexp, let ni) = _read_exponent(s, size, i)
          i = ni
          exp2 = pexp
        end
      end
      exp2 = (exp2 - (4 * frac.i64())) + (4 * dropped.i64())
    else
      var seen_point = false
      while i < size do
        let c = s._apply(i)
        if _is_digit(c) then
          any = true
          let d = (c - '0').u64()
          if seen_point then
            frac = frac + 1
          end
          if (d != 0) or (ndigits > 0) then
            if ndigits < _dec_digit_cap() then
              m.mul_add(10, d)
              ndigits = ndigits + 1
            else
              if d != 0 then
                sticky = true
              end
              dropped = dropped + 1
            end
          end
          i = i + 1
        elseif (c == '.') and not seen_point then
          seen_point = true
          i = i + 1
        else
          break
        end
      end
      if not any then
        error
      end
      if i < size then
        let c = s._apply(i)
        if (c == 'e') or (c == 'E') then
          (let dexp, let ni) = _read_exponent(s, size, i)
          i = ni
          exp10 = dexp
        end
      end
      exp10 = (exp10 - frac.i64()) + dropped.i64()
    end

    if i != size then
      error
    end

    if m.is_zero() then
      // All digits were zero: a signed zero regardless of the exponent.
      return (if neg then sign_bit else 0 end)
    end

    _round_to_float(
      m, neg, sign_bit, is_hex, exp10, exp2, sticky, p, bias)?

  fun _round_to_float(
    m: _BigUint,
    neg: Bool,
    sign_bit: U64,
    is_hex: Bool,
    exp10: I64,
    exp2: I64,
    sticky: Bool,
    p: USize,
    bias: I32)
    : U64 ?
  =>
    """
    Returns the IEEE bit pattern, sign included, of the value
    m * 10^exp10 (is_hex false) or m * 2^exp2 (is_hex true), rounded to p
    significand bits, ties to even. sticky marks nonzero digits dropped
    after m, which puts the value just above m * base^exp. Errors on
    overflow to infinity. m is consumed: the decimal path scales it in
    place.
    """
    let emax: I64 = bias.i64()
    let emin: I64 = 1 - bias.i64()
    let zero: U64 = if neg then sign_bit else 0 end

    // value = n * 2^s / d
    let n = m
    let d = _BigUint.from_u64(1)
    var s: I64 = exp2

    if not is_hex then
      // Reject or short-circuit extreme decimal exponents before
      // materializing 5^|exp10|: with at most _dec_digit_cap() digits in
      // m, past these bounds the value either exceeds the largest finite
      // float (an error) or is below half the least subnormal (a signed
      // zero).
      if bias > 200 then
        // f64: max ~1.8e308; with 800 digits in m, exp10 < -1124 gives a
        // value below 1e-324 < half of the least subnormal ~2.5e-324
        if exp10 > 308 then
          error
        end
        if exp10 < -1124 then
          return zero
        end
      else
        // f32: max ~3.4e38; with 800 digits in m, exp10 < -846 gives a
        // value below 1e-46 < half of the least subnormal ~7e-46
        if exp10 > 38 then
          error
        end
        if exp10 < -846 then
          return zero
        end
      end
      var pow5: I64 = if exp10 >= 0 then exp10 else -exp10 end
      if exp10 >= 0 then
        while pow5 > 0 do
          n.mul_add(5, 0)
          pow5 = pow5 - 1
        end
      else
        while pow5 > 0 do
          d.mul_add(5, 0)
          pow5 = pow5 - 1
        end
      end
      s = exp10
    else
      // Magnitude pre-checks on the exact binary value; they settle
      // overflow and deep underflow before any scaling or division.
      let e_est = (n.bit_length().i64() - 1) + s
      if e_est > (emax + 1) then
        error
      end
      if e_est < (emin - p.i64() - 1) then
        return zero
      end
    end

    // e0 is the binary exponent of the value or one more; the comparison
    // below decides. e is exact: 2^e <= value < 2^(e+1).
    let bl_n = n.bit_length()
    let bl_d = d.bit_length()
    let e0 = (bl_n.i64() - bl_d.i64()) + s
    let an = n.clone()
    let bd = d.clone()
    if s >= e0 then
      an.shl_in_place((s - e0).usize())
    else
      bd.shl_in_place((e0 - s).usize())
    end
    let e = if an.cmp(bd) >= 0 then e0 else e0 - 1 end

    // k is the exponent of one unit in the last significand place: the
    // quotient below is value / 2^k. Below emin the subnormal quantum is
    // fixed, so k stops at emin - p + 1.
    let k = (e.max(emin) - p.i64()) + 1

    // q = round(value / 2^k)
    let a = n.clone()
    // b aliases d, which is not read again.
    let b = d
    let sh = s - k
    if sh >= 0 then
      a.shl_in_place(sh.usize())
    else
      b.shl_in_place((-sh).usize())
    end

    let bl_a = a.bit_length()
    let bl_b = b.bit_length()
    if (bl_a > bl_b) and ((bl_a - bl_b) > 63) then
      // Internal invariant: value / 2^k < 2^(p+1), so q fits in p+1 bits
      // and this is unreachable. An internal bug would surface as a
      // parse error here.
      error
    end

    var q: U64 = 0
    let rem = _BigUint
    var idx = bl_a
    while idx > 0 do
      idx = idx - 1
      rem.mul_add(2, if a.bit(idx) then 1 else 0 end)
      q = q << 1
      if rem.cmp(b) >= 0 then
        rem.sub_in_place(b)
        q = q or 1
      end
    end

    let twice = rem.clone()
    twice.shl_in_place(1)
    let c = twice.cmp(b)
    if (c > 0) or ((c == 0) and (sticky or ((q and 1) == 1))) then
      q = q + 1
    end

    let top_bit = U64(1) << (p - 1).u64()
    var q_r = q
    var e_out = e
    if (e >= emin) and (q_r == (top_bit << 1)) then
      // Rounding carried into bit p; renormalize.
      q_r = top_bit
      e_out = e + 1
    end
    if e_out > emax then
      error
    end
    (let exp_field, let frac) =
      if e >= emin then
        ((e_out + bias.i64()).u64(), q_r - top_bit)
      elseif q_r >= top_bit then
        // Rounded up to the smallest normal
        (U64(1), q_r - top_bit)
      else
        (U64(0), q_r)
      end

    (if neg then sign_bit else 0 end) or
      (exp_field << (p - 1).u64()) or
      frac
