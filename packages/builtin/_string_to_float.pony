// Pure-Pony correctly-rounded String -> F32/F64 conversion.
//
// Replaces calls to the C library's `strtof`/`strtod` from `String.f32()` and
// `String.f64()`. The behavioral contract is "bit-identical to the C library
// the previous implementation called" — every input produces the same bits
// or the same `error` as `strtod`/`strtof` would on the build's libc.
//
// Pipeline:
//
//   String._ptr + (start, _size)
//     |
//     v
//   _StringToF{32,64}.parse
//     - _FloatLexer.skip_whitespace
//     - _FloatLexer.read_sign
//     - dispatch on lookahead to one of:
//         _HexFloat.f{32,64}        (0x/0X prefix)
//         _FloatSpecials.f{32,64}   (i/I/n/N prefix)
//         _DecimalToF{32,64}.parse  (otherwise)
//     - full-consumption check (next_index must equal end)
//     - sign-flip via Pony unary `-` (verified to lower to LLVM `fneg`,
//       producing correct sign-bit flips on +0, NaN, and Inf)
//
//   Internal stages return `(value, next_index)` tuples; all malformed-input
//   failures propagate as bare `?`. The one exception is `_clinger_f{32,64}`,
//   which returns `(F32|F64 | _SlowPathNeeded)` because "fast path declined"
//   is a routing signal distinct from "input was malformed."
//
// Sign-flip coupling: the orchestrator's `if negative then -mag else mag end`
// relies on Pony's unary `-` on F32/F64 lowering to LLVM `fneg`. Empirically
// verified on the current compiler/LLVM: `-(F64.from_bits(0x7FF8...))` yields
// `F64.from_bits(0xFFF8...)`, and `-(F64(0))` yields
// `F64.from_bits(0x8000...)`. Bit-pattern tests in builtin_test on `"-0"`,
// `"-NaN"`, `"-Inf"` guard this — if a future LLVM change breaks it, those
// tests fail loudly.
//
// File-wide bounds-guarding protocol: every helper in this file that reads
// from a `Pointer[U8] box` MUST guard `i < to` (or `(i + N) <= to` for
// N-byte lookahead) before each read. Every helper MUST accept `from == to`
// (empty range) without reading. This is the property that makes the
// pure-Pony parser safe on non-null-terminated String slices — the C
// `strtod`/`strtof` it replaces relied on null termination and was a
// source of bugs (#1445). Any future helper that violates this protocol
// reintroduces that bug class.

primitive _SlowPathNeeded
  """
  Routing signal returned by `_clinger_f{32,64}` to indicate the Clinger fast
  path cannot prove a correctly-rounded result for this input. Not an error
  type — the input is well-formed; the fast path is declining to round it.
  Distinct representation from `?` so the reader cannot confuse "fast path
  declined" with "input malformed" (CLAUDE.md principle 12).
  """

primitive _FloatLexer
  """
  Width-agnostic byte-cursor helpers. Every helper that reads `N` bytes from
  index `i` MUST guard `(i + N) <= to` before reading, and MUST accept
  `from == to` (empty range) without reading. This is the bounds-guarding
  protocol that protects against reading past `_size` on non-null-terminated
  String slices — the property `strtod`/`strtof` could not give us through
  `cstring()`.
  """
  fun skip_whitespace(ptr: Pointer[U8] box, from: USize, to: USize): USize =>
    """
    Advances past C-locale whitespace bytes: ' ', '\\t', '\\n', '\\v', '\\f',
    '\\r' (matches `isspace` and `strtod`'s leading-whitespace skip). Returns
    the new index. Idempotent on `from == to`.
    """
    var i = from
    while i < to do
      match ptr._apply(i)
      | ' ' | '\t' | '\n' | '\v' | '\f' | '\r' => i = i + 1
      else
        break
      end
    end
    i

  fun read_sign(ptr: Pointer[U8] box, from: USize, to: USize)
    : (Bool, USize)
  =>
    """
    Reads at most one sign byte. Returns `(negative, next_index)`.
    `'-'` -> `(true, from+1)`, `'+'` -> `(false, from+1)`, anything else (or
    `from == to`) -> `(false, from)`. The fact that "+" is consumed but not
    recorded matches `strtod`'s behavior.
    """
    if from >= to then return (false, from) end
    match ptr._apply(from)
    | '-' => (true, from + 1)
    | '+' => (false, from + 1)
    else
      (false, from)
    end

  fun looks_like_hex(ptr: Pointer[U8] box, from: USize, to: USize): Bool =>
    """
    True iff the next two bytes are "0" followed by "x"/"X". Bounds-guard
    requires `(from + 1) < to` before reading the second byte.
    """
    if (from + 1) >= to then return false end
    (ptr._apply(from) == '0')
      and ((ptr._apply(from + 1) and not 0x20) == 'X')

  fun looks_like_special(ptr: Pointer[U8] box, from: USize, to: USize): Bool
  =>
    """
    True iff the next byte starts an Inf/NaN sequence ('i'/'I'/'n'/'N'). One
    byte of lookahead; `_FloatSpecials` does the longest-match parse.
    """
    if from >= to then return false end
    match ptr._apply(from) and not 0x20
    | 'I' | 'N' => true
    else
      false
    end

primitive _StringToF32
  """
  Per-width orchestrator for `String.f32`. Owns whitespace, sign, dispatch,
  full-consumption check, and sign-flip; delegates the per-shape parse to
  `_HexFloat` / `_FloatSpecials` / `_DecimalToF32`.
  """
  fun parse(ptr: Pointer[U8] box, from: USize, to: USize): F32 ? =>
    let after_ws = _FloatLexer.skip_whitespace(ptr, from, to)
    (let negative, let after_sign) =
      _FloatLexer.read_sign(ptr, after_ws, to)
    (let mag, let next_index) =
      if _FloatLexer.looks_like_hex(ptr, after_sign, to) then
        _HexFloat.f32(ptr, after_sign, to)?
      elseif _FloatLexer.looks_like_special(ptr, after_sign, to) then
        _FloatSpecials.f32(ptr, after_sign, to)?
      else
        _DecimalToF32.parse(ptr, after_sign, to)?
      end
    if next_index != to then error end
    if negative then -mag else mag end

primitive _StringToF64
  """
  Per-width orchestrator for `String.f64`. Same shape as `_StringToF32` with
  `F64` substituted.
  """
  fun parse(ptr: Pointer[U8] box, from: USize, to: USize): F64 ? =>
    let after_ws = _FloatLexer.skip_whitespace(ptr, from, to)
    (let negative, let after_sign) =
      _FloatLexer.read_sign(ptr, after_ws, to)
    (let mag, let next_index) =
      if _FloatLexer.looks_like_hex(ptr, after_sign, to) then
        _HexFloat.f64(ptr, after_sign, to)?
      elseif _FloatLexer.looks_like_special(ptr, after_sign, to) then
        _FloatSpecials.f64(ptr, after_sign, to)?
      else
        _DecimalToF64.parse(ptr, after_sign, to)?
      end
    if next_index != to then error end
    if negative then -mag else mag end

primitive _DecimalToF32
  """
  Decimal-shape parser for F32. Calls width-agnostic `_DecimalLex.scan`,
  the per-width Clinger fast path; falls through to `_DecimalSlowPath` on
  `_SlowPathNeeded`. The slow path rounds directly to a 24-bit mantissa —
  there is no intermediate F64 in this path (would double-round).
  """
  fun parse(ptr: Pointer[U8] box, from: USize, to: USize): (F32, USize) ? =>
    (let sig, let exp10, let truncated, let next_index) =
      _DecimalLex.scan(ptr, from, to)?
    let value: F32 =
      match _clinger_f32(sig, exp10, truncated)
      | let v: F32 => v
      | _SlowPathNeeded =>
        _DecimalSlowPath(ptr, from, next_index).to_f32()?
      end
    (value, next_index)

  fun _clinger_f32(sig: U64, exp10: I32, truncated: Bool)
    : (F32 | _SlowPathNeeded)
  =>
    """
    Clinger fast path for F32. Eligibility (all three must hold):
      - `truncated == false` — the scanner kept every significant digit;
      - `sig <= 2^24` — fits exactly in the F32 mantissa (sig.f32() is
        the exact value);
      - `|exp10| <= 10` — `10^|exp10|` is exactly representable in F32
        (5^N fits in 24 bits iff N <= 10).
    Outside any of those, return `_SlowPathNeeded`. Inside, a single F32
    multiply or divide gives the correctly-rounded result (IEEE-754
    guarantees one operation with exact operands is correctly rounded).
    """
    if truncated then return _SlowPathNeeded end
    if sig > 0x1000000 /* 2^24 */ then return _SlowPathNeeded end
    if (exp10 > 10) or (exp10 < -10) then return _SlowPathNeeded end
    let sig_f = sig.f32()
    if exp10 == 0 then
      sig_f
    elseif exp10 > 0 then
      sig_f * _pow10_f32(exp10)
    else
      sig_f / _pow10_f32(-exp10)
    end

  fun _pow10_f32(n: I32): F32 =>
    """
    Exact F32 value of 10^n for n in [0, 10]. Builds via repeated multiply
    by 10.0 — each intermediate 10^k (k <= 10) is exactly representable in
    F32 because 5^k fits in 24 bits, so each multiply is exact (a single
    IEEE-correctly-rounded op with exact operands and an exact result).
    Caller has verified n is in range.
    """
    var r: F32 = 1.0
    var i: I32 = 0
    while i < n do
      r = r * 10.0
      i = i + 1
    end
    r

primitive _DecimalToF64
  """
  Decimal-shape parser for F64. See `_DecimalToF32`. The Clinger fast path's
  applicability requires significand <= 2^53, |exp10| <= 22, AND truncated
  == false; outside any of those, the slow path takes over.
  """
  fun parse(ptr: Pointer[U8] box, from: USize, to: USize): (F64, USize) ? =>
    (let sig, let exp10, let truncated, let next_index) =
      _DecimalLex.scan(ptr, from, to)?
    let value: F64 =
      match _clinger_f64(sig, exp10, truncated)
      | let v: F64 => v
      | _SlowPathNeeded =>
        _DecimalSlowPath(ptr, from, next_index).to_f64()?
      end
    (value, next_index)

  fun _clinger_f64(sig: U64, exp10: I32, truncated: Bool)
    : (F64 | _SlowPathNeeded)
  =>
    """
    Clinger fast path for F64. Eligibility (all three must hold):
      - `truncated == false`;
      - `sig <= 2^53` — fits exactly in the F64 mantissa;
      - `|exp10| <= 22` — `10^|exp10|` is exactly representable in F64
        (5^N fits in 53 bits iff N <= 22).
    """
    if truncated then return _SlowPathNeeded end
    if sig > 0x20000000000000 /* 2^53 */ then return _SlowPathNeeded end
    if (exp10 > 22) or (exp10 < -22) then return _SlowPathNeeded end
    let sig_f = sig.f64()
    if exp10 == 0 then
      sig_f
    elseif exp10 > 0 then
      sig_f * _pow10_f64(exp10)
    else
      sig_f / _pow10_f64(-exp10)
    end

  fun _pow10_f64(n: I32): F64 =>
    """
    Exact F64 value of 10^n for n in [0, 22]. See `_pow10_f32` for the
    proof of exactness. Caller has verified n is in range.
    """
    var r: F64 = 1.0
    var i: I32 = 0
    while i < n do
      r = r * 10.0
      i = i + 1
    end
    r

primitive _DecimalLex
  """
  Width-agnostic decimal-shape scanner. Used by both `_DecimalToF32` and
  `_DecimalToF64` to fold the integer + fraction + exponent into a single
  `(sig, exp10, truncated, next_index)` tuple. The Clinger fast path
  consumes this directly; the slow path re-scans from the raw bytes so it
  has access to digits past U64 capacity.
  """
  fun scan(ptr: Pointer[U8] box, from: USize, to: USize)
    : (U64 /* truncated significand */, I32 /* exp10 */,
       Bool /* truncated */, USize /* next_index */) ?
  =>
    """
    Walk decimal mantissa (integer + optional `.` + optional fraction) and
    optional `e`/`E` exponent. Accumulate digits into a U64 significand,
    setting `truncated` if any non-zero digit didn't fit. Compute the
    decimal exponent as

        exp10 = e_value + digits_dropped - digits_after_dot

    where `digits_dropped` counts how many digits the U64 couldn't hold
    (these represent low-order digits of the mantissa, so each contributes
    a factor of 10 to `exp10`) and `digits_after_dot` counts every fraction
    digit (kept or dropped).

    Errors only if no mantissa digits are present at all. `e`/`E` with no
    following decimal digits backtracks to before the `e`, matching glibc
    `strtod`.
    """
    var i = from
    var sig: U64 = 0
    var any_digit = false
    var truncated = false
    var digits_dropped: I32 = 0
    var digits_after_dot: I32 = 0

    // Integer digits.
    while i < to do
      let c = ptr._apply(i)
      if (c < '0') or (c > '9') then break end
      any_digit = true
      (sig, let dropped) = _fold_digit(sig, c - '0')
      if dropped then
        digits_dropped = digits_dropped + 1
        if c != '0' then truncated = true end
      end
      i = i + 1
    end

    // Optional fraction.
    if (i < to) and (ptr._apply(i) == '.') then
      i = i + 1
      while i < to do
        let c = ptr._apply(i)
        if (c < '0') or (c > '9') then break end
        any_digit = true
        digits_after_dot = digits_after_dot + 1
        (sig, let dropped) = _fold_digit(sig, c - '0')
        if dropped then
          digits_dropped = digits_dropped + 1
          if c != '0' then truncated = true end
        end
        i = i + 1
      end
    end

    if not any_digit then error end

    // Optional `e`/`E` exponent. No digits after `e` -> backtrack.
    var e_value: I32 = 0
    if (i < to) and ((ptr._apply(i) and 0xDF) == 'E') then
      let e_start = i
      i = i + 1
      (let e_neg, let after_sign) = _FloatLexer.read_sign(ptr, i, to)
      i = after_sign
      var any_e_digit = false
      while i < to do
        let c = ptr._apply(i)
        if (c < '0') or (c > '9') then break end
        any_e_digit = true
        // Saturate at a value beyond any meaningful float exponent —
        // anything above ~330 already saturates F64; ~5000 is safe
        // headroom that won't overflow I32 after the digits_dropped /
        // digits_after_dot adjustments below.
        if e_value < 100_000 then
          e_value = (e_value * 10) + (c - '0').i32()
        end
        i = i + 1
      end
      if not any_e_digit then
        i = e_start
        e_value = 0
      elseif e_neg then
        e_value = -e_value
      end
    end

    let exp10 = (e_value + digits_dropped) - digits_after_dot
    (sig, exp10, truncated, i)

  fun _fold_digit(sig: U64, digit: U8): (U64, Bool /* dropped */) =>
    """
    Try to accumulate one digit into `sig` (decimal-shift-and-add). Returns
    `(new_sig, false)` on success or `(sig, true)` if `sig * 10 + digit`
    would overflow `U64` — in which case the caller records that this is
    a dropped digit (contributing to `exp10` and possibly `truncated`).
    """
    (let after_mul, let mul_ovf) = sig.mulc(10)
    if mul_ovf then return (sig, true) end
    (let after_add, let add_ovf) = after_mul.addc(digit.u64())
    if add_ovf then return (sig, true) end
    (after_add, false)

class ref _DecimalSlowPath
  """
  Big-decimal scratch for the correctly-rounded slow path. The only mutable
  type in the pipeline; created locally inside `_DecimalToFx.parse`, used,
  then discarded — never crosses an actor boundary.

  Holds up to N decimal digits in a fixed-size buffer. The bound N comes
  from the maximum number of significant decimal digits that can
  distinguish a halfway case at the F64 subnormal floor, plus the
  headroom needed for shift-by-2 operations during normalization.
  Truncation past the buffer is recorded with a sticky bit so the
  rounding head treats the true value as strictly greater than the
  buffer contents — this sticky-bit safety holds ONLY when the buffer is
  at least N. If a future change reduces the buffer below N, this
  comment lies and the rounding is wrong.

  Derivation of N = 768:
    - F64 has 53 bits of mantissa; to disambiguate a halfway-rounding
      case at the most extreme exponent (the subnormal floor near
      2^-1074), the slow path may shift the binary value left by up to
      ~1074 bits to bring it into the normalized [2^52, 2^53) range.
      Each `multiply_by_2` can grow the decimal digit count by at most 1
      (multiplying by 2 carries at most one new high digit). So the
      buffer must hold initial_digits + 1074 / log2(10) ≈
      initial_digits + 324 digits.
    - F64 round-trip precision is ~17 decimal digits. We conservatively
      cap input ingestion at 768 - 1 = 767 *significant* digits; longer
      inputs set truncated/sticky at scan time. 767 + 1 carry slot gives
      the 768 bound. Rust's `core::num::dec2flt` uses the same constant
      for the same reason; agreement is a convergence signal rather than
      a copied magic value.
  """
  // Significant digits, MSB-first. The current size is always `_digits.size()`
  // — kept as a single source of truth rather than a parallel field that
  // could drift out of sync.
  let _digits: Array[U8] ref
  // Position of the implicit decimal point: number of digits in the
  // integer part. Negative for sub-unit values (e.g., after stripping
  // leading zeros in "0.001", the buffer holds [1] with _decimal_point
  // == -2: value = (1) * 10^(-2 - 1) = 1e-3).
  var _decimal_point: I32 = 0
  // True iff a non-zero digit was discarded — at scan time (input exceeded
  // buffer) or during a shift (multiply-by-2 ran out of buffer at the MSB
  // end, or divide-by-2 dropped a non-zero LSB).
  var _truncated: Bool = false

  new create(ptr: Pointer[U8] box, from: USize, to: USize) =>
    """
    Re-walk the decimal-shape input from [from, to) and populate the digit
    buffer. Called only when Clinger has declined, so the input is known
    well-formed by construction (`_DecimalLex.scan` already accepted it).
    """
    _digits = Array[U8].create(768)
    var i = from
    var seen_dot = false
    var leading_zero = true

    while i < to do
      let c = ptr._apply(i)
      if (c == '.') and (not seen_dot) then
        seen_dot = true
        i = i + 1
      elseif (c >= '0') and (c <= '9') then
        let d = c - '0'
        if leading_zero and (d == 0) then
          // Skip leading zeros. If we're past the dot, each skipped zero
          // pushes _decimal_point down by 1 (it sits before the first
          // significant digit). If we're before the dot, leading zeros
          // don't shift anything — "00123" and "123" are identical.
          if seen_dot then
            _decimal_point = _decimal_point - 1
          end
        else
          leading_zero = false
          if _digits.size() < 768 then
            _digits.push(d)
          else
            // Buffer full: discard the digit and record sticky if it's
            // non-zero. Correctness depends on the buffer (768 digits)
            // being large enough that the discarded tail can only push
            // the true value strictly past whatever's in the buffer —
            // see the type-level coupling comment for the derivation.
            if d != 0 then _truncated = true end
          end
          if not seen_dot then
            _decimal_point = _decimal_point + 1
          end
        end
        i = i + 1
      else
        break
      end
    end

    // Strip trailing zeros — they don't affect the value, and a trimmed
    // buffer makes the rounding step's "is the fraction zero?" check
    // trivially correct.
    while (_digits.size() > 0) and
      (try _digits(_digits.size() - 1)? == 0 else false end)
    do
      try _digits.pop()? end
    end

    // Optional `e`/`E` exponent. _DecimalLex has already accepted this
    // input, so we don't re-validate — just consume what's there.
    if (i < to) and ((ptr._apply(i) and 0xDF) == 'E') then
      i = i + 1
      (let e_neg, let after_sign) = _FloatLexer.read_sign(ptr, i, to)
      i = after_sign
      var e_value: I32 = 0
      while i < to do
        let c = ptr._apply(i)
        if (c < '0') or (c > '9') then break end
        if e_value < 100_000 then
          e_value = (e_value * 10) + (c - '0').i32()
        end
        i = i + 1
      end
      let signed = if e_neg then -e_value else e_value end
      _decimal_point = _decimal_point + signed
    end

  fun ref to_f64(): F64 ? =>
    """
    Normalize the buffer so the integer part lies in [2^52, 2^53); the
    binary exponent is tracked alongside. Then read the integer part as
    the IEEE mantissa, round based on the fraction part + sticky, and
    assemble the F64 bits.

    Rounds the exact value once at 52-bit precision; do NOT "optimize"
    this by computing F64 from a wider intermediate then narrowing — that
    double-rounds and breaks correctness on halfway cases.
    """
    F64.from_bits(_to_float(52, 1023, 0x7FE)?)

  fun ref to_f32(): F32 ? =>
    """
    Same shape as `to_f64`, with the F32 mantissa width and exponent
    bounds. The 24-bit mantissa is computed directly from the digit
    buffer — F32 NEVER routes through F64 in this path (that would
    double-round).
    """
    F32.from_bits(_to_float(23, 127, 0xFE)?.u32())

  fun ref _to_float(
    mantissa_bits: U64, exp_bias: I32, exp_max_biased: I32)
    : U64 ?
  =>
    """
    Big-decimal correctly-rounded conversion to a sign-free IEEE bit
    pattern at the target width (caller's responsibility to widen and
    apply the sign). Operates on the buffer in place: shifts the value
    in increments of 2 until the integer part is in [2^mantissa_bits,
    2^(mantissa_bits + 1)), then rounds with the residual fraction +
    sticky.

    Worst-case complexity: O(buffer_size × shift_count). For F64,
    shift_count is bounded by ~1075 (the range from `2^-1074` subnormal
    floor to `~2^1024` overflow); buffer_size is 768. So worst-case
    ~10^6 digit-byte operations per parse, which is acceptable for the
    slow path because Clinger handles the common case in O(1).
    """
    // The orchestrator routes here from non-zero inputs whose Clinger
    // declined the conversion. The "all-zero digit buffer" case can
    // still legitimately arise — e.g., "0e100" overflows Clinger's
    // |exp10| <= 22 bound on the way in, even though the value is
    // exactly zero. Return the +0.0 bit pattern; the orchestrator's
    // `-mag` applies the sign for "-0e100".
    if _digits.size() == 0 then return 0 end

    let target_low: U64 = U64(1) << mantissa_bits
    let target_high: U64 = U64(1) << (mantissa_bits + 1)
    var exp2: I32 = 0

    // Normalize: bring the integer part into [target_low, target_high).
    // The loop terminates because each shift moves the integer part by
    // a factor of 2 toward the target range, and the range is non-empty.
    while true do
      let int_digit_count = _decimal_point
      // If integer part has too many digits to fit in a U64, it is
      // unambiguously much larger than target_high — shift right.
      // 19 decimal digits is the largest count that always fits in U64.
      if int_digit_count >= 19 then
        _shift_right_1()
        exp2 = exp2 + 1
      elseif int_digit_count <= 0 then
        // Value < 1; mantissa target is >= 1. Shift left.
        _shift_left_1()
        exp2 = exp2 - 1
      else
        let ip = _integer_part_u64()
        if ip >= target_high then
          _shift_right_1()
          exp2 = exp2 + 1
        elseif ip < target_low then
          _shift_left_1()
          exp2 = exp2 - 1
        else
          break
        end
      end
      // Pre-rounding bounds check on exp2. The post-rounding biased
      // exponent will be `exp2 + mantissa_bits + exp_bias`; if we're
      // already well out of range, bail to avoid pathological shift
      // loops on extreme inputs (e.g., "1e-1000" shifts left ~3322
      // times if not stopped). The `+2`/`-2` margin leaves room for
      // both a rounding carry that nudges exp2 by 1 and the slop
      // between "integer part in target range" and the actual
      // post-rounding exponent — verified to never error on a value
      // that should round to a representable normal F64/F32.
      let biased_lower = (exp2 + mantissa_bits.i32()) + exp_bias
      if biased_lower > (exp_max_biased + 2) then error end
      if biased_lower < -2 then error end
    end

    var mantissa = _integer_part_u64()
    let cls = _fraction_class()

    let round_up =
      match cls
      | _RoundingAboveHalf => true
      | _RoundingBelowHalf => false
      | _RoundingExactlyHalf => (mantissa and 1) == 1
      end
    if round_up then mantissa = mantissa + 1 end

    if mantissa == target_high then
      mantissa = mantissa >> 1
      exp2 = exp2 + 1
    end

    let biased_exp = (exp2 + mantissa_bits.i32()) + exp_bias
    if biased_exp > exp_max_biased then error end
    if biased_exp <= 0 then error end

    (biased_exp.u64() << mantissa_bits) or (mantissa - target_low)

  fun ref _shift_left_1() =>
    """
    Multiply the value by 2: digit-by-digit RTL doubling with carry. If
    the final carry-out is non-zero, prepend it as a new MSB and bump
    `_decimal_point`. If the buffer is full and we'd need a new LSB
    digit, no — multiply by 2 never adds an LSB. It can however add an
    MSB. If the buffer is full at the MSB end when a carry-out arrives,
    the LSB is dropped (its value goes into the sticky tail) to make
    room for the new MSB; the design's buffer size is large enough that
    any dropped LSB is below the F64 rounding-decision region.
    """
    var carry: U8 = 0
    var idx = _digits.size()
    while idx > 0 do
      idx = idx - 1
      let d = try _digits(idx)? else 0 end
      let v = (d * 2) + carry
      try _digits(idx)? = v % 10 end
      carry = v / 10
    end
    if carry > 0 then
      // Insert a new MSB digit. If the buffer is full, drop the LSB to
      // make room — the dropped digit becomes part of the sticky tail
      // (correctness depends on the 768-digit buffer being large enough
      // that any dropped tail is below the F64 rounding-decision
      // region; see the type-level coupling comment).
      if _digits.size() >= 768 then
        _truncated = _truncated or
          (try _digits(_digits.size() - 1)? != 0 else false end)
        try _digits.pop()? end
      end
      _digits.unshift(carry)
      _decimal_point = _decimal_point + 1
    end

  fun ref _shift_right_1() =>
    """
    Divide the value by 2: digit-by-digit LTR with remainder. The final
    remainder bit (0 or 1) becomes a new LSB digit (0 or 5). If the
    buffer is full and we can't fit the new LSB, drop it and set
    `_truncated`. Strip a leading zero if the MSB became 0; this
    decrements `_decimal_point`.
    """
    var rem: U8 = 0
    var i: USize = 0
    let n = _digits.size()
    while i < n do
      let d = try _digits(i)? else 0 end
      let v = (rem * 10) + d
      try _digits(i)? = v / 2 end
      rem = v % 2
      i = i + 1
    end
    if rem != 0 then
      if _digits.size() < 768 then
        _digits.push(5)
      else
        // Dropping the half-bit at the LSB end: the discarded fraction
        // is exactly 1/2 of one buffer LSU, treated as sticky for
        // rounding correctness — same buffer-size precondition as the
        // input-truncation site (see the type-level coupling comment).
        _truncated = true
      end
    end
    // Strip a leading zero (created when the original MSB was a "1" that
    // halved to "0").
    if (_digits.size() > 0) and (try _digits(0)? == 0 else false end) then
      try _digits.shift()? end
      _decimal_point = _decimal_point - 1
    end

  fun box _integer_part_u64(): U64 =>
    """
    Read the integer part of the value (digits before the implicit
    decimal point) as a U64. Caller must ensure `_decimal_point ∈
    [0, 19]` to avoid overflow — the convergence loop checks for this
    before calling.
    """
    var v: U64 = 0
    let limit =
      if _decimal_point < 0 then USize(0)
      else _decimal_point.usize() end
    let n = _digits.size()
    var i: USize = 0
    while i < limit do
      let d = if i < n then
        try _digits(i)? else 0 end
      else
        // Trailing zeros that exist only by position (digit count is
        // less than _decimal_point).
        U8(0)
      end
      v = (v * 10) + d.u64()
      i = i + 1
    end
    v

  fun box _fraction_class(): _RoundingClass =>
    """
    Compare the fraction part (digits past the implicit decimal point) to
    exactly 0.5 (the rounding threshold). Returns BELOW / EXACTLY / ABOVE
    half. `_truncated` tilts EXACTLY to ABOVE (the truncated tail is
    non-zero, so the true value is strictly greater than what's in the
    buffer).
    """
    let int_count =
      if _decimal_point < 0 then USize(0)
      else _decimal_point.usize() end
    let n = _digits.size()
    // First fraction digit (at index int_count, if present).
    let first = if int_count < n then
      try _digits(int_count)? else 0 end
    else
      U8(0)
    end
    if first < 5 then return _RoundingBelowHalf end
    if first > 5 then return _RoundingAboveHalf end
    // first == 5: check if any subsequent digit is non-zero (would push
    // above half), or if truncated (which means the buffered tail is the
    // strict prefix of a longer true value, also above).
    var i = int_count + 1
    while i < n do
      if (try _digits(i)? else 0 end) != 0 then
        return _RoundingAboveHalf
      end
      i = i + 1
    end
    if _truncated then _RoundingAboveHalf else _RoundingExactlyHalf end

primitive _RoundingBelowHalf
primitive _RoundingExactlyHalf
primitive _RoundingAboveHalf
type _RoundingClass is
  (_RoundingBelowHalf | _RoundingExactlyHalf | _RoundingAboveHalf)

primitive _HexFloat
  """
  Hex-float parser. Hex mantissa is binary, so there is no decimal fast/slow
  split — only rounding to nearest-even at target precision when the hex
  mantissa has more significant bits than fit.

  Grammar (after the `0x`/`0X` prefix that the caller's `looks_like_hex`
  already verified):

      hex-digits [`.` hex-digits] [(`p`|`P`) [`+`|`-`] decimal-digits]

  At least one hex digit must be present (in the integer or fraction part).
  A `p`/`P` with no following decimal digits backtracks to before the `p`
  (matches glibc `strtod`, verified empirically).

  Mantissa accumulation: hex digits are folded left-to-right into a U64.
  Once the U64 is full, further integer digits set a sticky bit (and
  contribute 4 to the binary exponent each), further fraction digits set
  the sticky bit (with no exponent contribution since they were already
  beyond the precision boundary). The sticky bit preserves round-to-even
  correctness past the U64 width.

  Overflow / underflow rules match glibc `strtod`/`strtof` for hex:
  infinite results error; flush-to-zero from a non-zero input errors;
  subnormal results are RETURNED (no ERANGE). This differs from the
  decimal path, where every subnormal errors — glibc applies different
  rules to the two grammars, and "no behavior change" preserves that
  asymmetry. Zero from an explicit `0x0p0`-style input is returned.
  """
  fun f32(ptr: Pointer[U8] box, from: USize, to: USize): (F32, USize) ? =>
    // Rounds the exact value once at 24-bit precision via `_RoundFloat.f32`.
    // Do NOT "optimize" by computing F64 first and narrowing — that
    // double-rounds and breaks correctness on halfway cases.
    (let mantissa, let bin_exp, let sticky, let next_index) =
      _scan(ptr, from, to)
    if mantissa == 0 then
      (F32(0.0), next_index)
    else
      let result = _RoundFloat.f32(mantissa, bin_exp, sticky)?
      (result, next_index)
    end

  fun f64(ptr: Pointer[U8] box, from: USize, to: USize): (F64, USize) ? =>
    // Rounds the exact value once at 53-bit precision.
    (let mantissa, let bin_exp, let sticky, let next_index) =
      _scan(ptr, from, to)
    if mantissa == 0 then
      (F64(0.0), next_index)
    else
      let result = _RoundFloat.f64(mantissa, bin_exp, sticky)?
      (result, next_index)
    end

  fun _scan(ptr: Pointer[U8] box, from: USize, to: USize)
    : (U64 /* mantissa */, I32 /* binary exponent */,
       Bool /* sticky */, USize /* next_index */)
  =>
    """
    Lex `0x`-prefixed hex float into a `(mantissa, binary_exp, sticky,
    next_index)` tuple. The caller has verified `0x`/`0X` is present.

    Returns `(0, 0, false, from + 1)` when no hex digit follows the prefix
    — the orchestrator's full-consumption check then errors on the
    remaining `x`. This matches the glibc behavior where `"0x"` consumes
    only the `0`.
    """
    // Caller verified `looks_like_hex`, so (from + 1) < to and
    // ptr[from..from+1] is "0x"/"0X".
    var i = from + 2
    var mantissa: U64 = 0
    var sticky = false
    var any_digit = false
    // Each dropped integer digit contributes 4 to the binary exponent
    // (it represents a higher-order group of 4 bits that we couldn't fit).
    var int_digits_dropped: I32 = 0
    // Each KEPT fraction digit subtracts 4 from the binary exponent.
    var frac_digits_kept: I32 = 0

    // Integer hex digits.
    while i < to do
      let d = _hex_digit(ptr._apply(i))
      if d < 0 then break end
      any_digit = true
      if mantissa.clz() >= 4 then
        mantissa = (mantissa << 4) or d.u64()
      else
        if d != 0 then sticky = true end
        int_digits_dropped = int_digits_dropped + 1
      end
      i = i + 1
    end

    // Optional fraction.
    if (i < to) and (ptr._apply(i) == '.') then
      i = i + 1
      while i < to do
        let d = _hex_digit(ptr._apply(i))
        if d < 0 then break end
        any_digit = true
        if mantissa.clz() >= 4 then
          mantissa = (mantissa << 4) or d.u64()
          frac_digits_kept = frac_digits_kept + 1
        else
          if d != 0 then sticky = true end
        end
        i = i + 1
      end
    end

    // No digits at all: backtrack so only the `0` is consumed. The `x` (or
    // `X`) and anything after will fail the orchestrator's consumption
    // check.
    if not any_digit then return (0, 0, false, from + 1) end

    var bin_exp = (int_digits_dropped * 4) - (frac_digits_kept * 4)

    // Optional binary exponent: p|P [+|-] decimal-digits. No digits after
    // `p` -> backtrack to before `p` (matches glibc).
    if (i < to) and ((ptr._apply(i) and 0xDF) == 'P') then
      let p_start = i
      i = i + 1
      (let exp_negative, let after_sign) =
        _FloatLexer.read_sign(ptr, i, to)
      i = after_sign
      var exp_value: I32 = 0
      var any_exp_digit = false
      while i < to do
        let c = ptr._apply(i)
        if (c < '0') or (c > '9') then break end
        any_exp_digit = true
        // Saturate at a large bound to avoid I32 overflow. Anything past
        // ~5000 is already overflow/underflow territory; the rounder will
        // error regardless of the exact value.
        if exp_value < 100_000 then
          exp_value = (exp_value * 10) + (c - '0').i32()
        end
        i = i + 1
      end
      if not any_exp_digit then
        i = p_start
      else
        let signed = if exp_negative then -exp_value else exp_value end
        bin_exp = bin_exp + signed
      end
    end

    (mantissa, bin_exp, sticky, i)

  fun _hex_digit(c: U8): I8 =>
    """
    Returns 0..15 for a hex digit byte, -1 otherwise. The byte is checked
    against ASCII ranges directly rather than going through case-folding,
    so non-hex bytes are rejected without ambiguity.
    """
    if (c >= '0') and (c <= '9') then (c - '0').i8()
    elseif (c >= 'A') and (c <= 'F') then ((c - 'A') + 10).i8()
    elseif (c >= 'a') and (c <= 'f') then ((c - 'a') + 10).i8()
    else
      -1
    end

primitive _RoundFloat
  """
  Round a value of the form `mantissa * 2^binary_exp` (with `sticky`
  indicating that additional non-zero bits exist below the kept mantissa)
  to the target IEEE 754 float, applying round-to-nearest-ties-to-even.

  Errors on overflow, on flush-to-zero from a non-zero input, and on
  inexact subnormal results — matches the current `strtod`/`strtof`
  behavior of setting `errno=ERANGE` for those cases on glibc (verified
  empirically; this is the "no behavior change" contract). Exact
  subnormal results return successfully with the appropriate IEEE
  subnormal encoding.

  Preconditions: `mantissa > 0` (caller short-circuits zero). `sticky`
  may only be true if the caller has dropped non-zero bits below
  `mantissa`'s LSB — its meaning is "the true value is strictly greater
  than what's in `mantissa`."
  """
  fun f64(mantissa: U64, binary_exp: I32, sticky: Bool): F64 ? =>
    F64.from_bits(_round(mantissa, binary_exp, sticky,
      52 /* mantissa bits */, 1023 /* exp bias */,
      0x7FE /* max biased exp */, 11 /* round-tail bit count */)?)

  fun f32(mantissa: U64, binary_exp: I32, sticky: Bool): F32 ? =>
    F32.from_bits(
      _round(mantissa, binary_exp, sticky, 23, 127, 0xFE, 40)?.u32())

  fun _round(
    mantissa: U64, binary_exp: I32, sticky: Bool,
    mantissa_bits: U64, exp_bias: I32,
    exp_max_biased: I32, round_tail_bits: U64)
    : U64 /* assembled sign-free IEEE bits */ ?
  =>
    """
    Width-parameterized rounding core. Returns the assembled IEEE bit
    pattern (sign bit always zero — the orchestrator's `-mag` flips it
    for negative inputs). The biased exponent is also returned for the
    caller's sanity checks if needed.

    Subnormal handling: this function is called only by `_HexFloat`. For
    hex inputs, glibc `strtod`/`strtof` return correctly-rounded
    subnormals successfully (no ERANGE) — verified empirically on
    `0x1p-1074` → bits `0x1`. So this rounder returns subnormal bits for
    values in `[2^min_exp_unbiased, 2^min_normal_exp_unbiased)` rather
    than erroring. Only true flush-to-zero (a nonzero input that rounds
    to ±0) and overflow set the partial `?`.

    The decimal slow path (`_DecimalSlowPath._to_float`) does its own
    rounding and errors on every subnormal — that matches glibc's
    decimal `strtod` ERANGE-on-subnormal behavior. The asymmetry is
    intentional and matches glibc.

    Invariants:
      - `mantissa` is non-zero (caller's precondition).
      - After normalization, the leading 1 of `mantissa` sits at bit 63.
      - `round_tail_bits` = 63 - `mantissa_bits` (the count of bits below
        the kept-mantissa region; bit `round_tail_bits - 1` is the
        halfway-rounding bit).
    """
    // Normalize: shift left until bit 63 is set. After this:
    //   value = normalized * 2^(binary_exp - lz)
    // where `normalized` has bit 63 set.
    let lz = mantissa.clz()
    let normalized = mantissa << lz
    let norm_exp = binary_exp - lz.i32()

    // IEEE unbiased exponent of the value: the leading 1 of `normalized`
    // is at bit 63, so the value is in [2^(norm_exp + 63), 2^(norm_exp + 64)).
    // That makes the IEEE unbiased exponent `norm_exp + 63`.
    let unbiased_exp = norm_exp + 63
    var biased_exp = unbiased_exp + exp_bias

    // Pre-rounding overflow check (subnormal handled below).
    if biased_exp > exp_max_biased then error end

    if biased_exp <= 0 then
      // Subnormal range. Glibc's `strtod`/`strtof` sets `errno=ERANGE`
      // when a hex input rounds in the subnormal range AND the
      // conversion is inexact (rounding occurred). The previous
      // strtod-backed `String.f64`/`f32` propagated `errno != 0` as a
      // partial-function error, so to preserve "no behavior change" we
      // error on any inexact subnormal here. Exact subnormals
      // (e.g. `0x1p-1074`, `0x0.fffffffffffffp-1022`) — where the bits
      // below the mantissa precision are all zero — return successfully
      // with `errno=0` in glibc and the same here.
      //
      // The IEEE encoding is biased_exp_field = 0 with the mantissa
      // field carrying all the precision (no implicit leading 1). To
      // extract that mantissa, shift `normalized` right by an additional
      // `(1 - biased_exp)` bits beyond `round_tail_bits`. If the total
      // shift reaches 64, the value rounds to 0 (or to a tiny subnormal
      // that glibc errors on regardless) — error.
      //
      // Documented divergence from glibc: on inputs that round up to
      // the smallest normal from strictly above the halfway boundary
      // (e.g. `0x1.fffffffffffff8p-1023`), glibc returns success
      // (errno=0); we error because the conversion is technically
      // inexact at subnormal precision. This affects a small set of
      // boundary inputs and is the conservative side of the contract.
      let extra_shift = (1 - biased_exp).u64()
      let total_shift = round_tail_bits + extra_shift
      if total_shift >= 64 then error end
      let stail_mask = (U64(1) << total_shift) - 1
      let stail = normalized and stail_mask
      // Inexact iff any bit below the kept-mantissa region is non-zero,
      // or sticky from earlier (mantissa truncation upstream).
      if (stail != 0) or sticky then error end
      let skept = normalized >> total_shift
      if skept == 0 then error end
      // Exact subnormal. biased_exp_field = 0; mantissa field = skept.
      return skept
    end

    // Normal range. Split `normalized` into kept (top `mantissa_bits + 1`
    // bits) and tail (low `round_tail_bits` bits).
    let kept = normalized >> round_tail_bits
    let tail_mask = (U64(1) << round_tail_bits) - 1
    let tail = normalized and tail_mask
    let halfway = U64(1) << (round_tail_bits - 1)

    let round_up =
      if tail > halfway then
        true
      elseif tail < halfway then
        false
      else
        // Exactly halfway in the kept bits. Sticky tilts up; otherwise
        // round to even (round up iff the kept LSB is 1).
        sticky or ((kept and 1) == 1)
      end

    var final_mantissa = if round_up then kept + 1 else kept end
    // Rounding carry: if kept was all-ones in `mantissa_bits + 1` bits,
    // `+1` sets bit `mantissa_bits + 1`. Shift right and bump exp.
    let carry_bit = U64(1) << (mantissa_bits + 1)
    if (final_mantissa and carry_bit) != 0 then
      final_mantissa = final_mantissa >> 1
      biased_exp = biased_exp + 1
      if biased_exp > exp_max_biased then error end
    end

    // Strip the implicit leading 1 (bit at position `mantissa_bits`) to
    // get the fraction. Assemble: `biased_exp << mantissa_bits | fraction`.
    let fraction_mask = (U64(1) << mantissa_bits) - 1
    let fraction = final_mantissa and fraction_mask
    (biased_exp.u64() << mantissa_bits) or fraction

primitive _FloatSpecials
  """
  Case-insensitive Inf / Infinity / NaN parser. NaN with an optional
  parenthesized integer payload — the payload mantissa bits encode whatever
  the contemporaneous strtod/strtof do (probed empirically on glibc, not a
  grammar of our own choosing).

  Payload rule (glibc, verified empirically):
    * payload is a base-0 unsigned integer (decimal, or `0x...` hex,
      or `0...` octal — strtoull semantics);
    * non-digit characters cause strtoull to return 0; parens are still
      consumed;
    * payload overflow saturates to U64 max;
    * mantissa = `(payload & MANTISSA_MASK) | QUIET_BIT` — the quiet bit is
      always set;
    * an unterminated `(` causes glibc to backtrack and consume only the
      `nan` — the caller's full-consumption check then errors on the
      trailing `(...`.
  """
  fun f32(ptr: Pointer[U8] box, from: USize, to: USize): (F32, USize) ? =>
    (let bits, let next_index) = _parse(ptr, from, to,
      U64(0x7F800000) /* F32 exp_max, widened to U64 */,
      U64(0x007FFFFF) /* F32 mantissa mask: low 23 bits */,
      U64(0x00400000) /* F32 quiet bit: bit 22 */)?
    (F32.from_bits(bits.u32()), next_index)

  fun f64(ptr: Pointer[U8] box, from: USize, to: USize): (F64, USize) ? =>
    (let bits, let next_index) = _parse(ptr, from, to,
      U64(0x7FF0000000000000) /* F64 exp_max */,
      U64(0x000FFFFFFFFFFFFF) /* F64 mantissa mask: low 52 bits */,
      U64(0x0008000000000000) /* F64 quiet bit: bit 51 */)?
    (F64.from_bits(bits), next_index)

  fun _parse(
    ptr: Pointer[U8] box, from: USize, to: USize,
    exp_max_bits: U64, mantissa_mask: U64, quiet_bit: U64)
    : (U64 /* bits */, USize /* next_index */) ?
  =>
    """
    Shared driver: dispatch on the first character to inf or nan, build the
    target-width bit pattern. Both widths share the integer-domain
    representation; only the masks and the final `from_bits` differ.
    """
    if from >= to then error end
    let first = ptr._apply(from) and 0xDF
    if first == 'I' then
      // Try "INFINITY" (8 bytes); fall back to "INF" (3 bytes).
      if _match_ci(ptr, from, to, "NFINITY", 1) then
        (exp_max_bits, from + 8)
      elseif _match_ci(ptr, from, to, "NF", 1) then
        (exp_max_bits, from + 3)
      else
        error
      end
    elseif first == 'N' then
      // Must be "NAN".
      if not _match_ci(ptr, from, to, "AN", 1) then error end
      let after_nan = from + 3
      // Optional `(payload)`. If `(` is present but `)` is missing,
      // backtrack — consume only "nan".
      if (after_nan < to) and (ptr._apply(after_nan) == '(') then
        match _scan_paren_payload(ptr, after_nan + 1, to)
        | (let payload: U64, let after_close: USize) =>
          let bits = exp_max_bits or (payload and mantissa_mask) or quiet_bit
          (bits, after_close)
        | None =>
          // Unterminated paren — backtrack to just after "nan".
          (exp_max_bits or quiet_bit, after_nan)
        end
      else
        (exp_max_bits or quiet_bit, after_nan)
      end
    else
      error
    end

  fun _match_ci(
    ptr: Pointer[U8] box, from: USize, to: USize,
    tail: String, offset: USize)
    : Bool
  =>
    """
    Case-insensitive match of `tail` against `ptr[from+offset ..]`. The
    caller has already verified `ptr[from]`. The "ci" trick (`b and 0xDF`)
    folds ASCII letters to uppercase; works for the letters this is used
    against (target bytes are all uppercase ASCII letters; input bytes
    that don't match will fail the comparison regardless of folding).
    """
    if (from + offset + tail.size()) > to then return false end
    var i: USize = from + offset
    for target_byte in tail.values() do
      if (ptr._apply(i) and 0xDF) != target_byte then return false end
      i = i + 1
    end
    true

  fun _scan_paren_payload(
    ptr: Pointer[U8] box, from: USize, to: USize)
    : ((U64, USize) | None)
  =>
    """
    Scan `[from, to)` looking for a closing ')'. The payload grammar
    matches glibc's `nan` n-char-sequence: `[0-9A-Za-z_]*`. Any other
    byte inside the parens (whitespace, sign, `.`, etc.) causes a
    backtrack — return `None` so the caller consumes only `nan` and lets
    the orchestrator's full-consumption check reject the trailing
    `(...)` as malformed.

    Returns `None` if no closing paren is found before `to`, OR if any
    byte inside the parens isn't in the n-char-sequence set.

    The payload bytes themselves are then parsed as a base-0 unsigned
    integer (decimal, or `0x...` hex). Non-numeric but valid
    n-char-sequence input (e.g., `nan(abc)`) yields payload 0 — matches
    glibc.
    """
    // Find the closing paren first. While walking, also validate that
    // every byte is a valid n-char-sequence byte.
    var close = from
    while close < to do
      let c = ptr._apply(close)
      if c == ')' then break end
      let is_digit = (c >= '0') and (c <= '9')
      let is_upper = (c >= 'A') and (c <= 'Z')
      let is_lower = (c >= 'a') and (c <= 'z')
      let is_under = c == '_'
      if not (is_digit or is_upper or is_lower or is_under) then
        return None
      end
      close = close + 1
    end
    if close >= to then return None end
    // Parse the validated bytes as base-0 unsigned integer.
    let payload = _parse_payload_int(ptr, from, close)
    (payload, close + 1)

  fun _parse_payload_int(
    ptr: Pointer[U8] box, from: USize, to: USize)
    : U64
  =>
    """
    strtoull-like base-0 parse of [from, to). On any unparseable byte,
    return whatever has been accumulated so far (which is 0 for an
    immediate failure) — matches glibc's behavior of "stop at first
    invalid byte." On overflow, saturate to U64.max_value(). No leading
    whitespace or sign — strtoull(., ., 0) inside NaN() in glibc does
    not accept those.
    """
    if from >= to then return 0 end
    var i = from
    var base: U64 = 10
    // Detect prefix: "0x"/"0X" -> hex 16; leading "0" -> octal 8; else decimal.
    if (ptr._apply(i) == '0') and ((i + 1) < to) then
      let nxt = ptr._apply(i + 1) and 0xDF
      if nxt == 'X' then
        base = 16
        i = i + 2
        if i >= to then return 0 end  // "0x" alone is not a valid number
      else
        // Leading 0 — interpret as octal unless what follows is non-octal,
        // in which case strtoull falls back to "value is 0, stop here".
        // Simplest implementation: octal, and unparseable bytes terminate.
        base = 8
        i = i + 1
      end
    end
    var value: U64 = 0
    while i < to do
      let c = ptr._apply(i)
      let digit: U64 =
        if (c >= '0') and (c <= '9') then (c - '0').u64()
        elseif (c >= 'A') and (c <= 'F') then ((c - 'A') + 10).u64()
        elseif (c >= 'a') and (c <= 'f') then ((c - 'a') + 10).u64()
        else break
        end
      if digit >= base then break end
      // Multiply-add with overflow saturating to U64.max_value().
      (let prod, let mul_ovf) = value.mulc(base)
      if mul_ovf then return U64.max_value() end
      (let sum, let add_ovf) = prod.addc(digit)
      if add_ovf then return U64.max_value() end
      value = sum
      i = i + 1
    end
    value
