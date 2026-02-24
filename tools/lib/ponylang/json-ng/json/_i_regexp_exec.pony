class _StateSet
  """
  Efficient set of NFA state indices. O(1) add/contains via bool array,
  O(active) iteration via active list, O(active) clear.
  """
  let _member: Array[Bool] ref
  let _active: Array[USize] ref

  new create(size: USize) =>
    _member = Array[Bool].init(false, size)
    _active = Array[USize]

  fun ref add(idx: USize) =>
    try
      if not _member(idx)? then
        _member(idx)? = true
        _active.push(idx)
      end
    end

  fun contains(idx: USize): Bool =>
    try _member(idx)? else false end

  fun ref clear() =>
    for idx in _active.values() do
      try _member(idx)? = false end
    end
    _active.clear()

  fun values(): ArrayValues[USize, this->Array[USize]]^ =>
    _active.values()

primitive _RangeMatcher
  """Binary search on sorted (start, end) codepoint range arrays."""

  fun matches(
    cp: U32,
    ranges: Array[(U32, U32)] val,
    negated: Bool)
    : Bool
  =>
    let found = _binary_search(cp, ranges)
    if negated then not found else found end

  fun _binary_search(cp: U32, ranges: Array[(U32, U32)] val): Bool =>
    var lo: USize = 0
    var hi: USize = ranges.size()
    while lo < hi do
      let mid = (lo + hi) / 2
      try
        (let start, let end_cp) = ranges(mid)?
        if cp < start then
          hi = mid
        elseif cp > end_cp then
          lo = mid + 1
        else
          return true
        end
      else
        return false
      end
    end
    false

primitive _NFAExec
  """Thompson NFA simulation for is_match and search."""

  fun is_match(nfa: _NFA, input: String): Bool =>
    """
    Test if the entire input string matches the pattern.
    I-Regexp matching is implicitly anchored at both ends.
    """
    let current = _StateSet(nfa.num_states)
    let next = _StateSet(nfa.num_states)
    // Start with epsilon closure of start state
    _add_state(nfa, current, nfa.start_state)
    // Process each codepoint
    var offset: USize = 0
    while offset < input.size() do
      try
        (let cp, let len) = input.utf32(offset.isize())?
        for state_idx in current.values() do
          if _matches_char(nfa, state_idx, cp) then
            try _add_state(nfa, next, nfa.out1(state_idx)?) end
          end
        end
        current.clear()
        // Swap: copy next into current, clear next
        for s in next.values() do current.add(s) end
        next.clear()
        offset = offset + len.usize()
      else
        return false
      end
    end
    // Check if any current state is a match state
    _has_match(nfa, current)

  fun search(nfa: _NFA, input: String): Bool =>
    """
    Test if any substring of input matches the pattern.
    Uses the "add start state at each step" technique.
    """
    let current = _StateSet(nfa.num_states)
    let next = _StateSet(nfa.num_states)
    // Check empty pattern match at start
    _add_state(nfa, current, nfa.start_state)
    if _has_match(nfa, current) then return true end
    // Process each codepoint
    var offset: USize = 0
    while offset < input.size() do
      try
        (let cp, let len) = input.utf32(offset.isize())?
        // Add start state to try matching from this position
        _add_state(nfa, current, nfa.start_state)
        for state_idx in current.values() do
          if _matches_char(nfa, state_idx, cp) then
            try _add_state(nfa, next, nfa.out1(state_idx)?) end
          end
        end
        if _has_match(nfa, next) then return true end
        current.clear()
        for s in next.values() do current.add(s) end
        next.clear()
        offset = offset + len.usize()
      else
        return false
      end
    end
    false

  fun _matches_char(nfa: _NFA, state_idx: USize, cp: U32): Bool =>
    """Check if a state matches a given codepoint."""
    try
      match nfa.kinds(state_idx)?
      | _NFACharRange =>
        _RangeMatcher.matches(cp, nfa.ranges(state_idx)?, nfa.negated(state_idx)?)
      | _NFADot =>
        // Matches any character except \n and \r (XSD semantics)
        (cp != 0x0A) and (cp != 0x0D)
      else
        false
      end
    else
      false
    end

  fun _add_state(nfa: _NFA, set: _StateSet, idx: USize) =>
    """
    Add a state to the set, following epsilon transitions (split states).
    Uses iterative approach with explicit stack to avoid stack overflow.
    """
    if idx >= nfa.num_states then return end
    let stack = Array[USize]
    stack.push(idx)
    while stack.size() > 0 do
      try
        let s = stack.pop()?
        if (s >= nfa.num_states) or set.contains(s) then continue end
        set.add(s)
        match nfa.kinds(s)?
        | _NFASplit =>
          let o1 = nfa.out1(s)?
          let o2 = nfa.out2(s)?
          if o1 < nfa.num_states then stack.push(o1) end
          if o2 < nfa.num_states then stack.push(o2) end
        else
          None
        end
      end
    end

  fun _has_match(nfa: _NFA, set: _StateSet): Bool =>
    """Check if any state in the set is a match state."""
    for s in set.values() do
      try
        match nfa.kinds(s)?
        | _NFAMatch => return true
        end
      end
    end
    false
