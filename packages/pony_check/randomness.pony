use "random"

type _RandomnessMode is (_ModePlain | _ModeRecording | _ModeReplaying)

primitive _ModePlain
primitive _ModeRecording
primitive _ModeReplaying

class ref Randomness
  """
  All draw methods are partial: they error during replay when the recorded
  choice sequence is exhausted or when a type/range mismatch is detected.
  In plain mode (user-constructed Randomness), draws never error.

  Integer methods generate values in the closed interval [min, max].
  Floating-point methods scale onto [min, max]; `min` is always reachable,
  `max` is an approximate upper bound due to floating-point rounding.
  """
  let _random: Random
  var _mode: _RandomnessMode = _ModePlain
  var _choices: Array[_Choice val] iso = recover iso Array[_Choice val] end
  var _spans: Array[_Span val] iso = recover iso Array[_Span val] end
  var _replay_seq: Array[_Choice val] val = recover val Array[_Choice val] end
  var _replay_idx: USize = 0
  var _replay_exhausted_flag: Bool = false
  var _span_stack: Array[USize] ref = Array[USize]
  var _span_labels: Array[SpanLabel] ref = Array[SpanLabel]

  new ref create(seed1: U64 = 42, seed2: U64 = 0) =>
    _random = Rand(seed1, seed2)

  // --- Public draw methods ---
  fun ref u8(min: U8 = U8.min_value(), max: U8 = U8.max_value()): U8 ? =>
    """
    Generate a U8 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), min.i128())?.u8()

  fun ref u16(min: U16 = U16.min_value(), max: U16 = U16.max_value()): U16 ? =>
    """
    Generate a U16 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), min.i128())?.u16()

  fun ref u32(min: U32 = U32.min_value(), max: U32 = U32.max_value()): U32 ? =>
    """
    Generate a U32 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), min.i128())?.u32()

  fun ref u64(min: U64 = U64.min_value(), max: U64 = U64.max_value()): U64 ? =>
    """
    Generate a U64 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), min.i128())?.u64()

  fun ref u128(
    min: U128 = U128.min_value(),
    max: U128 = U128.max_value())
    : U128 ?
  =>
    """
    Generate a U128 in closed interval [min, max].
    """
    _draw_u128(min, max, min)?

  fun ref ulong(
    min: ULong = ULong.min_value(),
    max: ULong = ULong.max_value())
    : ULong ?
  =>
    """
    Generate a ULong in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), min.i128())?.ulong()

  fun ref usize(
    min: USize = USize.min_value(),
    max: USize = USize.max_value())
    : USize ?
  =>
    """
    Generate a USize in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), min.i128())?.usize()

  fun ref i8(min: I8 = I8.min_value(), max: I8 = I8.max_value()): I8 ? =>
    """
    Generate an I8 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), 0)?.i8()

  fun ref i16(min: I16 = I16.min_value(), max: I16 = I16.max_value()): I16 ? =>
    """
    Generate an I16 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), 0)?.i16()

  fun ref i32(min: I32 = I32.min_value(), max: I32 = I32.max_value()): I32 ? =>
    """
    Generate an I32 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), 0)?.i32()

  fun ref i64(min: I64 = I64.min_value(), max: I64 = I64.max_value()): I64 ? =>
    """
    Generate an I64 in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), 0)?.i64()

  fun ref i128(
    min: I128 = I128.min_value(),
    max: I128 = I128.max_value())
    : I128 ?
  =>
    """
    Generate an I128 in closed interval [min, max].
    """
    _draw_int(min, max, 0)?

  fun ref ilong(
    min: ILong = ILong.min_value(),
    max: ILong = ILong.max_value())
    : ILong ?
  =>
    """
    Generate an ILong in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), 0)?.ilong()

  fun ref isize(
    min: ISize = ISize.min_value(),
    max: ISize = ISize.max_value())
    : ISize ?
  =>
    """
    Generate an ISize in closed interval [min, max].
    """
    _draw_int(min.i128(), max.i128(), 0)?.isize()

  fun ref f32(min: F32 = 0.0, max: F32 = 1.0): F32 ? =>
    """
    Generate an F32 in the range from `min` to `max`.
    """
    _draw_float(min.f64(), max.f64())?.f32()

  fun ref f64(min: F64 = 0.0, max: F64 = 1.0): F64 ? =>
    """
    Generate an F64 in the range from `min` to `max`.
    """
    _draw_float(min, max)?

  fun ref bool(): Bool ? =>
    """
    Generate a random Bool value.
    """
    match \exhaustive\ _mode
    | _ModePlain =>
      let v = (_random.next() % 2) == 0
      v
    | _ModeRecording =>
      let v = (_random.next() % 2) == 0
      _choices.push(_BoolChoice(v))
      v
    | _ModeReplaying =>
      if _replay_idx >= _replay_seq.size() then
        _replay_exhausted_flag = true
        error
      end
      match _replay_seq(_replay_idx)?
      | let bc: _BoolChoice =>
        _replay_idx = _replay_idx + 1
        bc.value
      else
        error
      end
    end

  fun ref forced_bool(value: Bool): Bool ? =>
    """
    Record a predetermined Bool. The shrinker will not attempt to
    change this choice. Use when the outcome is fixed by the
    generator's structure (e.g., elements below a collection minimum).
    """
    match \exhaustive\ _mode
    | _ModePlain =>
      value
    | _ModeRecording =>
      _choices.push(_BoolChoice(value, true))
      value
    | _ModeReplaying =>
      if _replay_idx >= _replay_seq.size() then
        _replay_exhausted_flag = true
        error
      end
      match _replay_seq(_replay_idx)?
      | let bc: _BoolChoice =>
        _replay_idx = _replay_idx + 1
        value
      else
        error
      end
    end

  fun ref shuffle[T](array: Array[T] ref) ? =>
    """
    Shuffle the array in place using Fisher-Yates, recording one integer
    choice per element as the swap index.
    """
    let n = array.size()
    if n <= 1 then return end
    start_span(SpanShuffle)
    var i = n - 1
    while i > 0 do
      let j = usize(0, i)?
      try
        array.swap_elements(i, j)?
      else
        end_span()?
        error
      end
      i = i - 1
    end
    end_span()?

  // --- Public span tracking ---
  fun ref start_span(label: SpanLabel) =>
    """
    Mark the beginning of a structural span in the choice sequence.
    Spans let the shrinker understand generator structure — for example,
    which choices belong to a single collection element.
    """
    let pos =
      match \exhaustive\ _mode
      | _ModePlain => _choices.size()
      | _ModeRecording => _choices.size()
      | _ModeReplaying => _replay_idx
      end
    _span_stack.push(pos)
    _span_labels.push(label)

  fun ref end_span(discard: Bool = false) ? =>
    """
    Close the most recently opened span. If `discard` is true, the span
    is marked as discarded (e.g., a filter rejection).

    Errors if no span is open (mismatched start_span/end_span calls).
    """
    let start = _span_stack.pop()?
    let label = _span_labels.pop()?
    let end_pos =
      match \exhaustive\ _mode
      | _ModePlain => _choices.size()
      | _ModeRecording => _choices.size()
      | _ModeReplaying => _replay_idx
      end
    _spans.push(_Span(start, end_pos, label, discard))

  // --- Package-private mode control ---
  fun ref _start_recording() =>
    _mode = _ModeRecording
    _choices = recover iso Array[_Choice val] end
    _spans = recover iso Array[_Span val] end
    _span_stack = Array[USize]
    _span_labels = Array[SpanLabel]

  fun ref _replay(choices: Array[_Choice val] val) =>
    _mode = _ModeReplaying
    _replay_seq = choices
    _replay_idx = 0
    _replay_exhausted_flag = false
    _choices = recover iso Array[_Choice val] end
    _spans = recover iso Array[_Span val] end
    _span_stack = Array[USize]
    _span_labels = Array[SpanLabel]

  fun ref _reset() =>
    _mode = _ModePlain
    _choices = recover iso Array[_Choice val] end
    _spans = recover iso Array[_Span val] end
    _span_stack = Array[USize]
    _span_labels = Array[SpanLabel]
    _replay_seq = recover val Array[_Choice val] end
    _replay_idx = 0
    _replay_exhausted_flag = false

  fun ref _get_choices(): Array[_Choice val] val =>
    _choices =
      recover iso Array[_Choice val] end

  fun ref _get_spans(): Array[_Span val] val =>
    _spans =
      recover iso Array[_Span val] end

  fun _consumed(): USize =>
    _replay_idx

  // --- Internal draw helpers ---
  fun ref _draw_int(min: I128, max: I128, shrink_towards: I128): I128 ? =>
    match \exhaustive\ _mode
    | _ModePlain =>
      _raw_int(min, max)
    | _ModeRecording =>
      let v = _raw_int(min, max)
      let towards = shrink_towards.max(min).min(max)
      _choices.push(_IntChoice(v, min, max, towards))
      v
    | _ModeReplaying =>
      if _replay_idx >= _replay_seq.size() then
        _replay_exhausted_flag = true
        error
      end
      match _replay_seq(_replay_idx)?
      | let ic: _IntChoice =>
        _replay_idx = _replay_idx + 1
        ic.value.max(min).min(max)
      else
        error
      end
    end

  fun ref _draw_float(min: F64, max: F64): F64 ? =>
    match \exhaustive\ _mode
    | _ModePlain =>
      (_random.real() * (max - min)) + min
    | _ModeRecording =>
      let v = (_random.real() * (max - min)) + min
      _choices.push(_FloatChoice(v, min, max))
      v
    | _ModeReplaying =>
      if _replay_idx >= _replay_seq.size() then
        _replay_exhausted_flag = true
        error
      end
      match _replay_seq(_replay_idx)?
      | let fc: _FloatChoice =>
        _replay_idx = _replay_idx + 1
        fc.value.max(min).min(max)
      else
        error
      end
    end

  fun ref _draw_u128(min: U128, max: U128, shrink_towards: U128): U128 ? =>
    match \exhaustive\ _mode
    | _ModePlain =>
      _raw_u128(min, max)
    | _ModeRecording =>
      let v = _raw_u128(min, max)
      let towards = shrink_towards.max(min).min(max)
      _choices.push(_U128Choice(v, min, max, towards))
      v
    | _ModeReplaying =>
      if _replay_idx >= _replay_seq.size() then
        _replay_exhausted_flag = true
        error
      end
      match _replay_seq(_replay_idx)?
      | let uc: _U128Choice =>
        _replay_idx = _replay_idx + 1
        uc.value.max(min).min(max)
      else
        error
      end
    end

  fun ref _raw_u128(min: U128, max: U128): U128 =>
    if min == max then return min end
    let range: U128 = max - min
    if range <= U64.max_value().u128() then
      if range == U64.max_value().u128() then
        min + _random.u64().u128()
      else
        min + _random.int(range.u64() + 1).u128()
      end
    else
      let high = _random.u64()
      let low = _random.u64()
      let raw = (high.u128() << 64) or low.u128()
      if range == U128.max_value() then
        min + raw
      else
        min + (raw %% (range + 1))
      end
    end

  fun ref _raw_int(min: I128, max: I128): I128 =>
    """
    Generate a random I128 in [min, max] using the underlying PRNG.
    """
    if min == max then return min end
    let range: U128 = (max - min).u128()
    if range <= U64.max_value().u128() then
      if range == U64.max_value().u128() then
        min + _random.u64().i128()
      else
        min + _random.int(range.u64() + 1).i128()
      end
    else
      let high = _random.u64()
      let low = _random.u64()
      let raw = (high.u128() << 64) or low.u128()
      if range == U128.max_value() then
        min + raw.i128()
      else
        min + (raw %% (range + 1)).i128()
      end
    end
