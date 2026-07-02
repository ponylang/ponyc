// Filter expression AST types for JSONPath (RFC 9535 Section 2.3.5).
//
// These types represent the parsed filter expression tree stored inside
// _FilterSelector. All types are val — constructed at parse time and
// evaluated immutably against JSON documents.

primitive _Nothing
  """
  Represents the absence of a value from a singular query result.

  Distinct from `None` (JSON null) per RFC 9535: a missing key yields
  `_Nothing`, while a key mapped to `null` yields `None`. Two Nothings
  compare equal; Nothing compared to any value is false (except `!=`).
  """

// The result of evaluating a singular query: either a value or absence.
type _QueryResult is (JsonValue | _Nothing)

// --- Comparison operators ---

primitive _CmpEq
primitive _CmpNeq
primitive _CmpLt
primitive _CmpLte
primitive _CmpGt
primitive _CmpGte

type _ComparisonOp is
  (_CmpEq | _CmpNeq | _CmpLt | _CmpLte | _CmpGt | _CmpGte)

// --- Singular segments (name/index only, no wildcards/slices/descendants) ---

class val _SingularNameSegment
  """Select an object member by key in a singular query."""
  let name: String

  new val create(name': String) =>
    name = name'

class val _SingularIndexSegment
  """Select an array element by index in a singular query."""
  let index: I64

  new val create(index': I64) =>
    index = index'

type _SingularSegment is (_SingularNameSegment | _SingularIndexSegment)

// --- Singular queries (used in comparisons) ---

class val _RelSingularQuery
  """Singular query relative to the current node (@)."""
  let segments: Array[_SingularSegment] val

  new val create(segments': Array[_SingularSegment] val) =>
    segments = segments'

class val _AbsSingularQuery
  """Singular query relative to the document root ($)."""
  let segments: Array[_SingularSegment] val

  new val create(segments': Array[_SingularSegment] val) =>
    segments = segments'

type _SingularQuery is (_RelSingularQuery | _AbsSingularQuery)

// --- Comparables (what can appear on either side of a comparison) ---
// Includes literal values, singular queries, and ValueType-returning
// function expressions (length, count, value).

type _LiteralValue is (String | I64 | F64 | Bool | None)

type _Comparable is
  (_LiteralValue | _SingularQuery | _LengthExpr | _CountExpr | _ValueExpr)

// --- Filter queries (used in existence tests, can be non-singular) ---

class val _RelFilterQuery
  """General query relative to the current node (@)."""
  let segments: Array[_Segment] val

  new val create(segments': Array[_Segment] val) =>
    segments = segments'

class val _AbsFilterQuery
  """General query relative to the document root ($)."""
  let segments: Array[_Segment] val

  new val create(segments': Array[_Segment] val) =>
    segments = segments'

type _FilterQuery is (_RelFilterQuery | _AbsFilterQuery)

// --- Logical expression AST ---

class val _OrExpr
  """Logical OR: true if either operand is true."""
  let left: _LogicalExpr
  let right: _LogicalExpr

  new val create(left': _LogicalExpr, right': _LogicalExpr) =>
    left = left'
    right = right'

class val _AndExpr
  """Logical AND: true if both operands are true."""
  let left: _LogicalExpr
  let right: _LogicalExpr

  new val create(left': _LogicalExpr, right': _LogicalExpr) =>
    left = left'
    right = right'

class val _NotExpr
  """Logical NOT: inverts the operand."""
  let expr: _LogicalExpr

  new val create(expr': _LogicalExpr) =>
    expr = expr'

class val _ComparisonExpr
  """
  Comparison between two comparables.

  Both sides are `_Comparable` values: literals, singular queries,
  or ValueType function expressions (`length`, `count`, `value`).
  Non-singular queries are not allowed directly in comparisons
  per RFC 9535 — that constraint is enforced at the type level.
  """
  let left: _Comparable
  let op: _ComparisonOp
  let right: _Comparable

  new val create(
    left': _Comparable,
    op': _ComparisonOp,
    right': _Comparable)
  =>
    left = left'
    op = op'
    right = right'

class val _ExistenceExpr
  """
  Existence test: true if the filter query selects at least one node.

  Unlike comparisons, existence tests can use non-singular queries
  (wildcards, slices, descendants).
  """
  let query: _FilterQuery

  new val create(query': _FilterQuery) =>
    query = query'

// --- Function extension AST (RFC 9535 Section 2.4) ---

class val _MatchExpr
  """
  Full-string I-Regexp match (RFC 9535 Section 2.4.6).

  Returns LogicalType: true if the entire input string matches the
  pattern. Returns false if either argument is not a string or if the
  pattern is not a valid I-Regexp.
  """
  let input: _Comparable
  let pattern: _Comparable

  new val create(input': _Comparable, pattern': _Comparable) =>
    input = input'
    pattern = pattern'

class val _SearchExpr
  """
  Substring I-Regexp search (RFC 9535 Section 2.4.7).

  Returns LogicalType: true if any substring of the input matches the
  pattern. Returns false if either argument is not a string or if the
  pattern is not a valid I-Regexp.
  """
  let input: _Comparable
  let pattern: _Comparable

  new val create(input': _Comparable, pattern': _Comparable) =>
    input = input'
    pattern = pattern'

class val _LengthExpr
  """
  Length of a value (RFC 9535 Section 2.4.4).

  Returns ValueType: for strings, the number of Unicode scalar values;
  for arrays, the number of elements; for objects, the number of members.
  Returns Nothing for all other types.
  """
  let arg: _Comparable

  new val create(arg': _Comparable) =>
    arg = arg'

class val _CountExpr
  """
  Count of nodes in a nodelist (RFC 9535 Section 2.4.5).

  Returns ValueType: the number of nodes selected by the query.
  Always returns a non-negative integer.
  """
  let query: _FilterQuery

  new val create(query': _FilterQuery) =>
    query = query'

class val _ValueExpr
  """
  Extract a single value from a nodelist (RFC 9535 Section 2.4.8).

  Returns ValueType: the value if the nodelist contains exactly one node,
  Nothing otherwise (empty or multiple nodes).
  """
  let query: _FilterQuery

  new val create(query': _FilterQuery) =>
    query = query'

type _LogicalExpr is
  ( _OrExpr
  | _AndExpr
  | _NotExpr
  | _ComparisonExpr
  | _ExistenceExpr
  | _MatchExpr
  | _SearchExpr
  )
