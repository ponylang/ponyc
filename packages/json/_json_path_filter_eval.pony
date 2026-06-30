use iregex = "iregex"

// Work-stack task types for the iterative compound-expression walk in
// `_FilterEval._apply_compound` (see that method's docstring for why the
// `&&`/`||`/`!` tree is walked with an explicit heap stack instead of native
// recursion). A `*RightTask` preserves short-circuit by scheduling its right
// operand only when the left operand didn't already settle the result.

class val _EvalTask
  """Evaluate `expr`, leaving its Bool on the result stack."""
  let expr: _LogicalExpr
  new val create(expr': _LogicalExpr) => expr = expr'

primitive _NotTask
  """Negate the Bool on top of the result stack."""

class val _AndRightTask
  """Runs after `&&`'s left operand; evaluate `right` unless left was false."""
  let right: _LogicalExpr
  new val create(right': _LogicalExpr) => right = right'

class val _OrRightTask
  """Runs after `||`'s left operand; evaluate `right` unless left was true."""
  let right: _LogicalExpr
  new val create(right': _LogicalExpr) => right = right'

type _FilterTask is (_EvalTask | _NotTask | _AndRightTask | _OrRightTask)

primitive _FilterEval
  """
  Evaluate a filter expression against a current node and document root.

  Returns `true` if the expression matches, `false` otherwise. Evaluation
  never raises — type mismatches, missing keys, and out-of-bounds indices
  all produce well-defined results per RFC 9535.
  """

  fun apply(
    expr: _LogicalExpr,
    current: JsonValue,
    root: JsonValue)
    : Bool
  =>
    """
    Evaluate one logical expression.

    A leaf (a comparison, existence test, or `match`/`search`) is evaluated
    directly. This is the common filter shape and `apply` runs once per node in
    a nodelist, so the leaf path avoids the work-stack allocation; a leaf also
    can't grow the deep `&&`/`||`/`!` tree the work stack exists to handle. A
    compound node hands off to `_apply_compound`.
    """
    match \exhaustive\ expr
    | let e: _ComparisonExpr =>
      _FilterCompare(e.left, e.op, e.right, current, root)
    | let e: _ExistenceExpr => _eval_existence(e.query, current, root)
    | let e: _MatchExpr => _eval_match(e, current, root)
    | let e: _SearchExpr => _eval_search(e, current, root)
    | let _: (_OrExpr | _AndExpr | _NotExpr) =>
      _apply_compound(expr, current, root)
    end

  fun _apply_compound(
    expr: _LogicalExpr,
    current: JsonValue,
    root: JsonValue)
    : Bool
  =>
    """
    Evaluate a compound `&&`/`||`/`!` expression with an explicit work stack.

    The tree is walked iteratively so that a long chain — which the parser
    builds in a loop without overflowing, then hands here as a deeply nested
    tree — is bounded by the heap rather than the scheduler thread's native
    stack. Short-circuit semantics match the recursive form: each
    `_AndExpr`/`_OrExpr` schedules its right operand only when the left operand
    did not already settle the result. Leaf operands are evaluated by `apply`
    (its direct, allocation-free path). Evaluation never raises, so the overflow
    cannot be reported as an error — it has to be removed, not capped.
    """
    let work = Array[_FilterTask]
    let results = Array[Bool]
    work.push(_EvalTask(expr))
    while work.size() > 0 do
      let task = try work.pop()? else _Unreachable(); return false end
      match \exhaustive\ task
      | let t: _EvalTask =>
        match \exhaustive\ t.expr
        | let e: _OrExpr =>
          work.push(_OrRightTask(e.right))
          work.push(_EvalTask(e.left))
        | let e: _AndExpr =>
          work.push(_AndRightTask(e.right))
          work.push(_EvalTask(e.left))
        | let e: _NotExpr =>
          work.push(_NotTask)
          work.push(_EvalTask(e.expr))
        | let e: _ComparisonExpr => results.push(apply(e, current, root))
        | let e: _ExistenceExpr => results.push(apply(e, current, root))
        | let e: _MatchExpr => results.push(apply(e, current, root))
        | let e: _SearchExpr => results.push(apply(e, current, root))
        end
      | _NotTask =>
        let v = try results.pop()? else _Unreachable(); return false end
        results.push(not v)
      | let t: _AndRightTask =>
        let v = try results.pop()? else _Unreachable(); return false end
        if v then
          work.push(_EvalTask(t.right)) // left true: result is right
        else
          results.push(false) // left false: short-circuit
        end
      | let t: _OrRightTask =>
        let v = try results.pop()? else _Unreachable(); return false end
        if v then
          results.push(true) // left true: short-circuit
        else
          work.push(_EvalTask(t.right)) // left false: result is right
        end
      end
    end
    try results(0)? else _Unreachable(); false end

  fun _eval_existence(
    query: _FilterQuery,
    current: JsonValue,
    root: JsonValue)
    : Bool
  =>
    """True if the query selects at least one node."""
    let results = match \exhaustive\ query
    | let q: _RelFilterQuery =>
      _JsonPathEval(current, root, q.segments)
    | let q: _AbsFilterQuery =>
      _JsonPathEval(root, root, q.segments)
    end
    results.size() > 0

  fun _eval_singular(
    query: _SingularQuery,
    current: JsonValue,
    root: JsonValue)
    : _QueryResult
  =>
    """
    Evaluate a singular query, returning the single value or `_Nothing`
    if no value exists at that path.
    """
    var node: _QueryResult = match \exhaustive\ query
    | let q: _RelSingularQuery => current
    | let q: _AbsSingularQuery => root
    end
    let segs = match \exhaustive\ query
    | let q: _RelSingularQuery => q.segments
    | let q: _AbsSingularQuery => q.segments
    end
    for seg in segs.values() do
      match \exhaustive\ node
      | let j: JsonValue =>
        node = match \exhaustive\ seg
        | let ns: _SingularNameSegment =>
          match j
          | let obj: JsonObject =>
            try obj(ns.name)? else _Nothing end
          else
            _Nothing
          end
        | let is': _SingularIndexSegment =>
          match j
          | let arr: JsonArray =>
            let idx = is'.index
            let effective = if idx >= 0 then
              idx.usize()
            else
              let abs_idx = idx.abs().usize()
              if abs_idx <= arr.size() then
                arr.size() - abs_idx
              else
                return _Nothing
              end
            end
            try arr(effective)? else _Nothing end
          else
            _Nothing
          end
        end
      | _Nothing => return _Nothing
      end
    end
    node

  fun _eval_match(
    expr: _MatchExpr,
    current: JsonValue,
    root: JsonValue)
    : Bool
  =>
    """
    Evaluate match(): full-string I-Regexp match.
    Returns false if either argument is not a string or if the pattern
    is not a valid I-Regexp.
    """
    let input_val = _FilterCompare._resolve(expr.input, current, root)
    let pattern_val = _FilterCompare._resolve(expr.pattern, current, root)
    match (input_val, pattern_val)
    | (let input_str: String, let pattern_str: String) =>
      match \exhaustive\ iregex.IRegexpCompiler.parse(pattern_str)
      | let re: iregex.IRegexp => re.is_match(input_str)
      | let _: iregex.IRegexpParseError => false
      end
    else
      false
    end

  fun _eval_search(
    expr: _SearchExpr,
    current: JsonValue,
    root: JsonValue)
    : Bool
  =>
    """
    Evaluate search(): substring I-Regexp search.
    Returns false if either argument is not a string or if the pattern
    is not a valid I-Regexp.
    """
    let input_val = _FilterCompare._resolve(expr.input, current, root)
    let pattern_val = _FilterCompare._resolve(expr.pattern, current, root)
    match (input_val, pattern_val)
    | (let input_str: String, let pattern_str: String) =>
      match \exhaustive\ iregex.IRegexpCompiler.parse(pattern_str)
      | let re: iregex.IRegexp => re.search(input_str)
      | let _: iregex.IRegexpParseError => false
      end
    else
      false
    end


primitive _FilterCompare
  """
  RFC 9535 comparison semantics for filter expressions.

  Handles Nothing (absent query result), type-specific equality and
  ordering, deep equality for arrays/objects, mixed I64/F64 comparison,
  and cross-type comparisons (always false, no coercion).
  """

  fun apply(
    left: _Comparable,
    op: _ComparisonOp,
    right: _Comparable,
    current: JsonValue,
    root: JsonValue)
    : Bool
  =>
    let lval = _resolve(left, current, root)
    let rval = _resolve(right, current, root)
    match \exhaustive\ op
    | _CmpEq  => _eq(lval, rval)
    | _CmpNeq => not _eq(lval, rval)
    | _CmpLt  => _lt(lval, rval)
    | _CmpLte => _lt(lval, rval) or _eq(lval, rval)
    | _CmpGt  => _lt(rval, lval)
    | _CmpGte => _lt(rval, lval) or _eq(lval, rval)
    end

  fun _resolve(
    c: _Comparable,
    current: JsonValue,
    root: JsonValue)
    : _QueryResult
  =>
    """Resolve a comparable to a concrete value or Nothing."""
    match \exhaustive\ c
    | let s: String => s
    | let n: I64 => n
    | let n: F64 => n
    | let b: Bool => b
    | None => None
    | let q: _RelSingularQuery =>
      _FilterEval._eval_singular(q, current, root)
    | let q: _AbsSingularQuery =>
      _FilterEval._eval_singular(q, current, root)
    | let e: _LengthExpr => _eval_length(e.arg, current, root)
    | let e: _CountExpr => _eval_count(e.query, current, root)
    | let e: _ValueExpr => _eval_value(e.query, current, root)
    end

  fun _eval_length(
    arg: _Comparable,
    current: JsonValue,
    root: JsonValue)
    : _QueryResult
  =>
    """
    Evaluate length(): Unicode scalar value count for strings,
    element count for arrays, member count for objects, Nothing otherwise.
    """
    let val' = _resolve(arg, current, root)
    match val'
    | let s: String =>
      // Count Unicode scalar values (codepoints) via UTF-32 iteration
      var count: I64 = 0
      var offset: USize = 0
      while offset < s.size() do
        try
          (_, let len) = s.utf32(offset.isize())?
          offset = offset + len.usize()
          count = count + 1
        else
          break
        end
      end
      count
    | let arr: JsonArray => arr.size().i64()
    | let obj: JsonObject => obj.size().i64()
    else
      _Nothing
    end

  fun _eval_count(
    query: _FilterQuery,
    current: JsonValue,
    root: JsonValue)
    : _QueryResult
  =>
    """Evaluate count(): cardinality of the nodelist from a query."""
    let results = match \exhaustive\ query
    | let q: _RelFilterQuery =>
      _JsonPathEval(current, root, q.segments)
    | let q: _AbsFilterQuery =>
      _JsonPathEval(root, root, q.segments)
    end
    results.size().i64()

  fun _eval_value(
    query: _FilterQuery,
    current: JsonValue,
    root: JsonValue)
    : _QueryResult
  =>
    """
    Evaluate value(): extract a single value from a nodelist.
    Returns the value if exactly one node, Nothing otherwise.
    """
    let results = match \exhaustive\ query
    | let q: _RelFilterQuery =>
      _JsonPathEval(current, root, q.segments)
    | let q: _AbsFilterQuery =>
      _JsonPathEval(root, root, q.segments)
    end
    if results.size() == 1 then
      try results(0)? else _Nothing end
    else
      _Nothing
    end

  fun _eq(left: _QueryResult, right: _QueryResult): Bool =>
    """
    RFC 9535 equality.

    Nothing == Nothing is true. Nothing vs any value is false.
    Same-type primitives use value equality. Mixed I64/F64 converts
    I64 to F64. Arrays compare element-wise; objects compare by same key
    set with equal values (iteration order doesn't matter) — both via the
    structural `_deep_eq` walk. Cross-type is false.
    """
    match (left, right)
    | (_Nothing, _Nothing) => true
    | (_Nothing, _) => false
    | (_, _Nothing) => false
    | (let a: I64, let b: I64) => a == b
    | (let a: F64, let b: F64) => a == b
    | (let a: I64, let b: F64) => a.f64() == b
    | (let a: F64, let b: I64) => a == b.f64()
    | (let a: String, let b: String) => a == b
    | (let a: Bool, let b: Bool) => a == b
    | (None, None) => true
    | (let a: JsonArray, let b: JsonArray) => _deep_eq(a, b)
    | (let a: JsonObject, let b: JsonObject) => _deep_eq(a, b)
    else
      false
    end

  fun _lt(left: _QueryResult, right: _QueryResult): Bool =>
    """
    RFC 9535 ordering.

    Anything involving Nothing is false. Numbers (including mixed
    I64/F64) use mathematical ordering. Strings use Unicode scalar
    value lexicographic ordering. All other types and cross-type
    comparisons are false.
    """
    match (left, right)
    | (_Nothing, _) => false
    | (_, _Nothing) => false
    | (let a: I64, let b: I64) => a < b
    | (let a: F64, let b: F64) => a < b
    | (let a: I64, let b: F64) => a.f64() < b
    | (let a: F64, let b: I64) => a < b.f64()
    | (let a: String, let b: String) => a < b
    else
      false
    end

  fun _deep_eq(a: JsonValue, b: JsonValue): Bool =>
    """
    Structural equality for JsonValue values.

    Arrays compare element-wise; objects compare by same key set with equal
    values (iteration order doesn't matter — JsonObject is backed by a CHAMP
    map). Walked with an explicit work stack rather than native recursion so
    that nesting depth is bounded by the heap, not the scheduler thread's
    native stack, which deeply nested input would otherwise overflow.
    """
    let stack = Array[(JsonValue, JsonValue)]
    stack.push((a, b))
    while stack.size() > 0 do
      (let x, let y) = try stack.pop()? else _Unreachable(); return false end
      match (x, y)
      | (let p: I64, let q: I64) => if p != q then return false end
      | (let p: F64, let q: F64) => if p != q then return false end
      | (let p: I64, let q: F64) => if p.f64() != q then return false end
      | (let p: F64, let q: I64) => if p != q.f64() then return false end
      | (let p: String, let q: String) => if p != q then return false end
      | (let p: Bool, let q: Bool) => if p != q then return false end
      | (None, None) => None
      | (let p: JsonArray, let q: JsonArray) =>
        if p.size() != q.size() then return false end
        var i: USize = 0
        while i < p.size() do
          let xv = try p(i)? else _Unreachable(); return false end
          let yv = try q(i)? else _Unreachable(); return false end
          stack.push((xv, yv))
          i = i + 1
        end
      | (let p: JsonObject, let q: JsonObject) =>
        if p.size() != q.size() then return false end
        for (key, pv) in p.pairs() do
          // A missing key means the objects differ — not an impossible path.
          let qv = try q(key)? else return false end
          stack.push((pv, qv))
        end
      else
        return false
      end
    end
    true
