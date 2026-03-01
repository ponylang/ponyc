// NFA state kind primitives
primitive _NFASplit
  """Epsilon branch: follow both out1 and out2."""
primitive _NFACharRange
  """Match codepoint in sorted ranges, then follow out1."""
primitive _NFADot
  """
  Match any codepoint except newline (U+000A) and carriage return (U+000D),
  then follow out1. Per XSD regex semantics (RFC 9485).
  """
primitive _NFAMatch
  """Accepting state."""

type _NFAStateKind is (_NFASplit | _NFACharRange | _NFADot | _NFAMatch)

class val _NFA
  """
  Compiled Thompson NFA. Parallel arrays indexed by state number.
  Immutable after construction.
  """
  let kinds: Array[_NFAStateKind] val
  let out1: Array[USize] val
  let out2: Array[USize] val
  let ranges: Array[Array[(U32, U32)] val] val
  let negated: Array[Bool] val
  let num_states: USize
  let start_state: USize

  new val _create(
    kinds': Array[_NFAStateKind] val,
    out1': Array[USize] val,
    out2': Array[USize] val,
    ranges': Array[Array[(U32, U32)] val] val,
    negated': Array[Bool] val,
    start': USize)
  =>
    kinds = kinds'
    out1 = out1'
    out2 = out2'
    ranges = ranges'
    negated = negated'
    num_states = kinds'.size()
    start_state = start'

class _NFAFragment
  """
  Intermediate result during NFA construction. Tracks a start state and
  dangling outputs that need to be patched to the next state.
  Each output is (state_index, is_out2).
  """
  let start: USize
  let outs: Array[(USize, Bool)] ref

  new create(start': USize, outs': Array[(USize, Bool)] ref) =>
    start = start'
    outs = outs'

class _NFABuildCtx
  """
  Mutable NFA construction context. Builds parallel arrays of state data.

  build_node() and _build_quantified() are partial: they raise when the NFA
  exceeds _max_states (10,000). Using partiality rather than a union return
  type keeps the recursive Thompson construction code concise — every recursive
  call simply propagates with `?` instead of requiring match/unwrap at each
  step. The caller (_NFABuilder.build) propagates the error to _IRegexpCompiler,
  which converts it to an _IRegexpParseError.
  """
  var _kinds: Array[_NFAStateKind] ref = Array[_NFAStateKind]
  var _out1: Array[USize] ref = Array[USize]
  var _out2: Array[USize] ref = Array[USize]
  var _ranges: Array[Array[(U32, U32)] val] ref = Array[Array[(U32, U32)] val]
  var _negated: Array[Bool] ref = Array[Bool]
  let _max_states: USize = 10_000
  let _none: USize = USize.max_value()

  fun ref _add_split(o1: USize, o2: USize): USize =>
    let idx = _kinds.size()
    _kinds.push(_NFASplit)
    _out1.push(o1)
    _out2.push(o2)
    _ranges.push(recover val Array[(U32, U32)] end)
    _negated.push(false)
    idx

  fun ref _add_char(r: Array[(U32, U32)] val, neg: Bool): USize =>
    let idx = _kinds.size()
    _kinds.push(_NFACharRange)
    _out1.push(_none)
    _out2.push(_none)
    _ranges.push(r)
    _negated.push(neg)
    idx

  fun ref _add_dot(): USize =>
    let idx = _kinds.size()
    _kinds.push(_NFADot)
    _out1.push(_none)
    _out2.push(_none)
    _ranges.push(recover val Array[(U32, U32)] end)
    _negated.push(false)
    idx

  fun ref _add_match(): USize =>
    let idx = _kinds.size()
    _kinds.push(_NFAMatch)
    _out1.push(_none)
    _out2.push(_none)
    _ranges.push(recover val Array[(U32, U32)] end)
    _negated.push(false)
    idx

  fun ref patch(outs: Array[(USize, Bool)] ref, target: USize) =>
    """
    Set all dangling outputs to point to target state.

    The array accesses here are guaranteed valid: every (idx, is_out2) pair
    in outs was created by _add_split/_add_char/_add_dot which pushed to both
    _out1 and _out2. An out-of-bounds index would indicate a bug in the NFA
    builder, not a runtime condition.
    """
    for (idx, is_out2) in outs.values() do
      try
        if is_out2 then
          _out2(idx)? = target
        else
          _out1(idx)? = target
        end
      else
        _Unreachable()
      end
    end

  fun state_count(): USize =>
    _kinds.size()

  fun ref build_node(node: _RegexNode): _NFAFragment ? =>
    """Compile an AST node to an NFA fragment via Thompson construction."""
    if _kinds.size() > _max_states then error end
    match \exhaustive\ node
    | let lit: _Literal =>
      let ranges' = recover val [(lit.codepoint, lit.codepoint)] end
      let s = _add_char(ranges', false)
      _NFAFragment(s, [(s, false)])
    | _Dot =>
      let s = _add_dot()
      _NFAFragment(s, [(s, false)])
    | let cc: _CharClass =>
      let s = _add_char(cc.ranges, cc.negated)
      _NFAFragment(s, [(s, false)])
    | let alt: _Alternation =>
      let left = build_node(alt.left)?
      let right = build_node(alt.right)?
      let s = _add_split(left.start, right.start)
      let outs = _merge_outs(left.outs, right.outs)
      _NFAFragment(s, outs)
    | let cat: _Concatenation =>
      let left = build_node(cat.left)?
      let right = build_node(cat.right)?
      patch(left.outs, right.start)
      _NFAFragment(left.start, right.outs)
    | let q: _Quantified =>
      _build_quantified(q)?
    | let g: _Group =>
      build_node(g.expr)?
    | _EmptyMatch =>
      // Split state with both outputs dangling — patches to same target
      let s = _add_split(_none, _none)
      _NFAFragment(s, [(s, false); (s, true)])
    end

  fun ref _build_quantified(q: _Quantified): _NFAFragment ? =>
    """Build NFA for quantified expression."""
    let min_count = q.min_count
    match \exhaustive\ q.max_count
    | None =>
      // Unbounded: {min,}
      if min_count == 0 then
        // * (zero or more)
        let e = build_node(q.expr)?
        let s = _add_split(e.start, _none)
        patch(e.outs, s) // loop back
        _NFAFragment(s, [(s, true)])
      elseif min_count == 1 then
        // + (one or more)
        let e = build_node(q.expr)?
        let s = _add_split(e.start, _none)
        patch(e.outs, s) // loop back
        _NFAFragment(e.start, [(s, true)])
      else
        // {n,} where n >= 2: (n-1) required copies + one + copy
        var result = build_node(q.expr)?
        var i: USize = 1
        while i < (min_count - 1) do
          let next = build_node(q.expr)?
          patch(result.outs, next.start)
          result = _NFAFragment(result.start, next.outs)
          i = i + 1
        end
        // Last copy with + semantics
        let last = build_node(q.expr)?
        patch(result.outs, last.start)
        let s = _add_split(last.start, _none)
        patch(last.outs, s)
        _NFAFragment(result.start, [(s, true)])
      end
    | let max_count: USize =>
      if (min_count == 0) and (max_count == 0) then
        // {0,0} or {0} — match empty string
        let s = _add_split(_none, _none)
        _NFAFragment(s, [(s, false); (s, true)])
      elseif (min_count == 0) and (max_count == 1) then
        // ? (zero or one)
        let e = build_node(q.expr)?
        let s = _add_split(e.start, _none)
        _NFAFragment(s, _merge_outs(e.outs, [(s, true)]))
      else
        // General {n,m}: n required copies + (m-n) optional copies
        var result: (_NFAFragment | None) = None
        var i: USize = 0
        // Build n required copies
        while i < min_count do
          let e = build_node(q.expr)?
          match \exhaustive\ result
          | let prev: _NFAFragment =>
            patch(prev.outs, e.start)
            result = _NFAFragment(prev.start, e.outs)
          | None =>
            result = e
          end
          i = i + 1
        end
        // Build (max - min) optional copies
        let opt_count = max_count - min_count
        i = 0
        while i < opt_count do
          let e = build_node(q.expr)?
          let s = _add_split(e.start, _none)
          let exit_outs: Array[(USize, Bool)] ref = [(s, true)]
          match \exhaustive\ result
          | let prev: _NFAFragment =>
            patch(prev.outs, s)
            result = _NFAFragment(prev.start, _merge_outs(e.outs, exit_outs))
          | None =>
            result = _NFAFragment(s, _merge_outs(e.outs, exit_outs))
          end
          i = i + 1
        end
        match result
        | let r: _NFAFragment => r
        else
          // Should not happen (min_count + opt_count > 0)
          error
        end
      end
    end

  fun ref _merge_outs(
    a: Array[(USize, Bool)] ref,
    b: Array[(USize, Bool)] ref)
    : Array[(USize, Bool)] ref
  =>
    let result = Array[(USize, Bool)](a.size() + b.size())
    for o in a.values() do result.push(o) end
    for o in b.values() do result.push(o) end
    result

  fun ref to_nfa(start: USize): _NFA =>
    """Freeze mutable ref arrays to val by copying into iso arrays."""
    let sz = _kinds.size()
    let k = recover iso Array[_NFAStateKind](sz) end
    let o1 = recover iso Array[USize](sz) end
    let o2 = recover iso Array[USize](sz) end
    let r = recover iso Array[Array[(U32, U32)] val](sz) end
    let n = recover iso Array[Bool](sz) end
    var i: USize = 0
    while i < sz do
      try
        k.push(_kinds(i)?)
        o1.push(_out1(i)?)
        o2.push(_out2(i)?)
        r.push(_ranges(i)?)
        n.push(_negated(i)?)
      else
        _Unreachable()
      end
      i = i + 1
    end
    _NFA._create(consume k, consume o1, consume o2, consume r, consume n,
      start)

primitive _NFABuilder
  """Build an NFA from a _RegexNode AST."""

  fun build(ast: _RegexNode): _NFA ? =>
    let ctx: _NFABuildCtx ref = _NFABuildCtx
    let frag = ctx.build_node(ast)?
    let match_state = ctx._add_match()
    ctx.patch(frag.outs, match_state)
    ctx.to_nfa(frag.start)
