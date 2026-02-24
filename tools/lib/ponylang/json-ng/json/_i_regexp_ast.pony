type _RegexNode is
  ( _Literal
  | _Dot
  | _CharClass
  | _Alternation
  | _Concatenation
  | _Quantified
  | _Group
  | _EmptyMatch )

class val _Literal
  """Single Unicode codepoint match."""
  let codepoint: U32

  new val create(codepoint': U32) =>
    codepoint = codepoint'

primitive _Dot
  """
  Matches any character except newline (U+000A) and carriage return (U+000D),
  per XSD regex semantics (RFC 9485 defers to XSD-2).
  """

class val _CharClass
  """
  Character class match against sorted, non-overlapping (start, end) codepoint
  ranges (inclusive). When negated, matches any codepoint NOT in the ranges.
  """
  let ranges: Array[(U32, U32)] val
  let negated: Bool

  new val create(ranges': Array[(U32, U32)] val, negated': Bool) =>
    ranges = ranges'
    negated = negated'

class val _Alternation
  """Alternation: matches left or right."""
  let left: _RegexNode
  let right: _RegexNode

  new val create(left': _RegexNode, right': _RegexNode) =>
    left = left'
    right = right'

class val _Concatenation
  """Concatenation: matches left followed by right."""
  let left: _RegexNode
  let right: _RegexNode

  new val create(left': _RegexNode, right': _RegexNode) =>
    left = left'
    right = right'

class val _Quantified
  """
  Quantified expression: matches expr between min_count and max_count times.
  max_count of None means unbounded (e.g., `*` is (0, None), `+` is (1, None)).
  """
  let expr: _RegexNode
  let min_count: USize
  let max_count: (USize | None)

  new val create(
    expr': _RegexNode,
    min_count': USize,
    max_count': (USize | None))
  =>
    expr = expr'
    min_count = min_count'
    max_count = max_count'

class val _Group
  """Grouping wrapper (no capture semantics)."""
  let expr: _RegexNode

  new val create(expr': _RegexNode) =>
    expr = expr'

primitive _EmptyMatch
  """Matches the empty string. Used for empty branches in alternation."""
