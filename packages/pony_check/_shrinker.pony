class ref _Shrinker
  """
  Implements choice-sequence-based internal shrinking.

  Given a failing choice sequence, produces candidate sequences that are
  shortlex-smaller and attempts to replay the generator against each one.
  If replay succeeds and the property still fails, the candidate becomes
  the new baseline. Repeats until no more reductions are found or the
  budget is exhausted.

  Shrinking passes (in order):
  1. Delete spans — remove entire structural units
  2. Lower choices toward shrink_towards — binary search each choice
  3. Redistribute adjacent int choices — shift weight toward earlier choice
  4. Shorten sequence — truncate trailing choices
  5. Sort adjacent spans — swap out-of-order span pairs
  """
  var _choices: Array[_Choice val] val
  var _spans: Array[_Span val] val
  var _reductions: USize = 0
  let _max_reductions: USize

  new ref create(
    choices: Array[_Choice val] val,
    spans: Array[_Span val] val,
    max_reductions: USize = 100)
  =>
    _choices = choices
    _spans = spans
    _max_reductions = max_reductions

  fun ref candidates(): Iterator[Array[_Choice val] val] =>
    """
    Return an iterator that lazily produces candidate choice sequences.
    Each accepted candidate (via `accept`) updates the baseline.
    """
    _ShrinkCandidates(this)

  fun ref accept(
    choices: Array[_Choice val] val,
    spans: Array[_Span val] val)
  =>
    _choices = choices
    _spans = spans
    _reductions = _reductions + 1

  fun ref budget_remaining(): Bool =>
    _reductions < _max_reductions

  fun current_choices(): Array[_Choice val] val =>
    _choices

  fun current_spans(): Array[_Span val] val =>
    _spans

class ref _ShrinkCandidates is Iterator[Array[_Choice val] val]
  let _shrinker: _Shrinker ref
  var _pass: USize = 0
  var _pass_iter: Iterator[Array[_Choice val] val] = _EmptyCandidates

  new ref create(shrinker: _Shrinker ref) =>
    _shrinker = shrinker
    _advance_pass()

  fun ref has_next(): Bool =>
    if not _shrinker.budget_remaining() then return false end
    if _pass_iter.has_next() then return true end
    _advance_pass()
    _pass_iter.has_next()

  fun ref next(): Array[_Choice val] val ? =>
    if not has_next() then error end
    _pass_iter.next()?

  fun ref _advance_pass() =>
    while _pass < 5 do
      _pass = _pass + 1
      _pass_iter =
        match _pass
        | 1 => _delete_spans_pass()
        | 2 => _lower_choices_pass()
        | 3 => _redistribute_pass()
        | 4 => _shorten_pass()
        | 5 => _sort_spans_pass()
        else
          _EmptyCandidates
        end
      if _pass_iter.has_next() then return end
    end

  fun ref _delete_spans_pass(): Iterator[Array[_Choice val] val] =>
    _DeleteSpansIter(_shrinker)

  fun ref _lower_choices_pass(): Iterator[Array[_Choice val] val] =>
    _LowerChoicesIter(_shrinker)

  fun ref _redistribute_pass(): Iterator[Array[_Choice val] val] =>
    _RedistributeIter(_shrinker)

  fun ref _shorten_pass(): Iterator[Array[_Choice val] val] =>
    _ShortenIter(_shrinker)

  fun ref _sort_spans_pass(): Iterator[Array[_Choice val] val] =>
    _SortSpansIter(_shrinker)

class ref _EmptyCandidates is Iterator[Array[_Choice val] val]
  fun ref has_next(): Bool => false
  fun ref next(): Array[_Choice val] val ? => error

class ref _DeleteSpansIter is Iterator[Array[_Choice val] val]
  """
  Try deleting each non-discarded span. Produces one candidate per span,
  with the span's choices removed from the sequence.
  """
  let _shrinker: _Shrinker ref
  var _idx: USize = 0

  new ref create(shrinker: _Shrinker ref) =>
    _shrinker = shrinker

  fun ref has_next(): Bool =>
    _skip_invalid()
    _idx < _shrinker.current_spans().size()

  fun ref next(): Array[_Choice val] val ? =>
    if not has_next() then error end
    let span = _shrinker.current_spans()(_idx)?
    _idx = _idx + 1
    _delete_span(span)?

  fun ref _skip_invalid() =>
    try
      let spans = _shrinker.current_spans()
      while _idx < spans.size() do
        let span = spans(_idx)?
        if span.discarded then
          _idx = _idx + 1
        else
          break
        end
      end
    end

  fun _delete_span(span: _Span val): Array[_Choice val] val ? =>
    let choices = _shrinker.current_choices()
    if span.end_index > choices.size() then error end
    let result =
      recover iso
        Array[_Choice val](
          choices.size() - (span.end_index - span.start_index))
      end
    var i: USize = 0
    while i < span.start_index do
      result.push(choices(i)?)
      i = i + 1
    end
    i = span.end_index
    while i < choices.size() do
      result.push(choices(i)?)
      i = i + 1
    end
    consume result

class ref _LowerChoicesIter is Iterator[Array[_Choice val] val]
  """
  For each IntChoice, binary-search its value toward shrink_towards.
  For each BoolChoice with value true, try false.

  After producing a candidate, the iterator checks whether the shrinker's
  baseline changed (candidate accepted) or stayed the same (rejected).
  On acceptance: hi = mid (search lower). On rejection: lo = mid (search
  higher toward the boundary where the property starts failing).
  """
  let _shrinker: _Shrinker ref
  var _idx: USize = 0
  var _lo: I128 = 0
  var _hi: I128 = 0
  var _active: Bool = false
  var _pending_mid: (I128 | None) = None

  new ref create(shrinker: _Shrinker ref) =>
    _shrinker = shrinker

  fun ref has_next(): Bool =>
    _resolve_pending()
    if _active then return true end
    _advance()
    _active

  fun ref next(): Array[_Choice val] val ? =>
    if not has_next() then error end
    let choices = _shrinker.current_choices()
    let choice = choices(_idx)?

    match choice
    | let ic: _IntChoice =>
      let mid = _lo + ((_hi - _lo) / 2)
      let candidate =
        _replace_choice(
          _idx,
          _IntChoice(mid, ic.min, ic.max, ic.shrink_towards))?
      if mid == _lo then
        _active = false
        _idx = _idx + 1
        _pending_mid = None
      else
        _pending_mid = mid
      end
      candidate
    | let bc: _BoolChoice =>
      _active = false
      _idx = _idx + 1
      _pending_mid = None
      _replace_choice(_idx - 1, _BoolChoice(false))?
    else
      _active = false
      _idx = _idx + 1
      _pending_mid = None
      error
    end

  fun ref _resolve_pending() =>
    match _pending_mid
    | let mid: I128 =>
      try
        let choices = _shrinker.current_choices()
        match choices(_idx)?
        | let ic: _IntChoice =>
          if ic.value == mid then
            _hi = mid
          else
            _lo = mid
          end
        end
      end
      _pending_mid = None
    end

  fun ref _advance() =>
    try
      let choices = _shrinker.current_choices()
      while _idx < choices.size() do
        match choices(_idx)?
        | let ic: _IntChoice =>
          if ic.value != ic.shrink_towards then
            _lo = ic.shrink_towards
            _hi = ic.value
            _active = true
            return
          end
        | let bc: _BoolChoice =>
          if bc.value then
            _active = true
            return
          end
        end
        _idx = _idx + 1
      end
    end

  fun _replace_choice(idx: USize, new_choice: _Choice val)
    : Array[_Choice val] val ?
  =>
    let choices = _shrinker.current_choices()
    let result = recover iso Array[_Choice val](choices.size()) end
    var i: USize = 0
    while i < choices.size() do
      if i == idx then
        result.push(new_choice)
      else
        result.push(choices(i)?)
      end
      i = i + 1
    end
    consume result

class ref _RedistributeIter is Iterator[Array[_Choice val] val]
  """
  For adjacent IntChoice pairs, try shifting value from the later
  toward the earlier (closer to shrink_towards for both).
  """
  let _shrinker: _Shrinker ref
  var _idx: USize = 0

  new ref create(shrinker: _Shrinker ref) =>
    _shrinker = shrinker

  fun ref has_next(): Bool =>
    _skip_invalid()
    (_idx + 1) < _shrinker.current_choices().size()

  fun ref next(): Array[_Choice val] val ? =>
    if not has_next() then error end
    let choices = _shrinker.current_choices()
    let ic1 = choices(_idx)? as _IntChoice
    let ic2 = choices(_idx + 1)? as _IntChoice

    _idx = _idx + 1

    let total = try ic1.value +? ic2.value else return next()? end
    let new1 =
      try
        ic1.shrink_towards.max(ic1.min).min(ic1.max)
          .max(total -? ic2.max).min(total -? ic2.min)
      else
        return next()?
      end
    let new2 = try total -? new1 else return next()? end

    let result = recover iso Array[_Choice val](choices.size()) end
    var i: USize = 0
    while i < choices.size() do
      if i == (_idx - 1) then
        result.push(_IntChoice(new1, ic1.min, ic1.max, ic1.shrink_towards))
      elseif i == _idx then
        result.push(_IntChoice(new2, ic2.min, ic2.max, ic2.shrink_towards))
      else
        result.push(choices(i)?)
      end
      i = i + 1
    end
    consume result

  fun ref _skip_invalid() =>
    try
      let choices = _shrinker.current_choices()
      while (_idx + 1) < choices.size() do
        match (choices(_idx)?, choices(_idx + 1)?)
        | (let _: _IntChoice, let _: _IntChoice) => return
        end
        _idx = _idx + 1
      end
      _idx = choices.size()
    end

class ref _ShortenIter is Iterator[Array[_Choice val] val]
  """
  Try progressively shorter prefixes of the choice sequence.
  """
  let _shrinker: _Shrinker ref
  var _len: USize

  new ref create(shrinker: _Shrinker ref) =>
    _shrinker = shrinker
    _len =
      if shrinker.current_choices().size() > 0 then
        shrinker.current_choices().size() - 1
      else
        0
      end

  fun ref has_next(): Bool =>
    _len > 0

  fun ref next(): Array[_Choice val] val ? =>
    if not has_next() then error end
    let choices = _shrinker.current_choices()
    let result = recover iso Array[_Choice val](_len) end
    var i: USize = 0
    while i < _len do
      result.push(choices(i)?)
      i = i + 1
    end
    _len = _len / 2
    consume result

class ref _SortSpansIter is Iterator[Array[_Choice val] val]
  """
  Try swapping adjacent out-of-order span pairs. Two spans are out of
  order when the first is shortlex-greater than the second.
  """
  let _shrinker: _Shrinker ref
  var _idx: USize = 0

  new ref create(shrinker: _Shrinker ref) =>
    _shrinker = shrinker

  fun ref has_next(): Bool =>
    _skip_invalid()
    (_idx + 1) < _shrinker.current_spans().size()

  fun ref next(): Array[_Choice val] val ? =>
    if not has_next() then error end
    let spans = _shrinker.current_spans()
    let s1 = spans(_idx)?
    let s2 = spans(_idx + 1)?
    _idx = _idx + 1

    if not _is_adjacent(s1, s2) then error end
    if not _shortlex_greater(s1, s2) then error end

    _swap_spans(s1, s2)?

  fun ref _skip_invalid() =>
    try
      let spans = _shrinker.current_spans()
      while (_idx + 1) < spans.size() do
        let s1 = spans(_idx)?
        let s2 = spans(_idx + 1)?
        if _is_adjacent(s1, s2) and _shortlex_greater(s1, s2) then
          return
        end
        _idx = _idx + 1
      end
    end

  fun _is_adjacent(s1: _Span val, s2: _Span val): Bool =>
    s1.end_index == s2.start_index

  fun _shortlex_greater(s1: _Span val, s2: _Span val): Bool =>
    let len1 = s1.end_index - s1.start_index
    let len2 = s2.end_index - s2.start_index
    if len1 != len2 then
      len1 > len2
    else
      _lex_greater(s1, s2)
    end

  fun _lex_greater(s1: _Span val, s2: _Span val): Bool =>
    let choices = _shrinker.current_choices()
    var i: USize = 0
    let len =
      (s1.end_index - s1.start_index).min(
        s2.end_index - s2.start_index)
    try
      while i < len do
        let c1 = choices(s1.start_index + i)?
        let c2 = choices(s2.start_index + i)?
        match (c1, c2)
        | (let ic1: _IntChoice, let ic2: _IntChoice) =>
          if ic1.value > ic2.value then return true end
          if ic1.value < ic2.value then return false end
        | (let bc1: _BoolChoice, let bc2: _BoolChoice) =>
          if (bc1.value and (not bc2.value)) then return true end
          if ((not bc1.value) and bc2.value) then return false end
        | (let fc1: _FloatChoice, let fc2: _FloatChoice) =>
          if fc1.value > fc2.value then return true end
          if fc1.value < fc2.value then return false end
        else
          return false
        end
        i = i + 1
      end
    end
    false

  fun ref _swap_spans(s1: _Span val, s2: _Span val)
    : Array[_Choice val] val ?
  =>
    let choices = _shrinker.current_choices()
    let result = recover iso Array[_Choice val](choices.size()) end

    var i: USize = 0
    while i < s1.start_index do
      result.push(choices(i)?)
      i = i + 1
    end

    i = s2.start_index
    while i < s2.end_index do
      result.push(choices(i)?)
      i = i + 1
    end

    i = s1.start_index
    while i < s1.end_index do
      result.push(choices(i)?)
      i = i + 1
    end

    i = s2.end_index
    while i < choices.size() do
      result.push(choices(i)?)
      i = i + 1
    end
    consume result
