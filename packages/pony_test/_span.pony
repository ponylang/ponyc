primitive SpanShuffle
  """
  Shuffle operations.
  """
  fun string(): String => "shuffle"

primitive SpanFilter
  """
  Filter rejection spans.
  """
  fun string(): String => "filter"

primitive SpanElement
  """
  Collection element spans.
  """
  fun string(): String => "element"

type SpanLabel is (SpanShuffle | SpanFilter | SpanElement)
  """
  Labels that identify what kind of structure a span represents.
  Pass one to `Randomness.start_span` when building custom generators.
  """

class val _Span is (Equatable[_Span] & Stringable)
  let start_index: USize
  let end_index: USize
  let label: SpanLabel
  let discarded: Bool

  new val create(
    start_index': USize,
    end_index': USize,
    label': SpanLabel,
    discarded': Bool = false)
  =>
    start_index = start_index'
    end_index = end_index'
    label = label'
    discarded = discarded'

  fun eq(other: box->_Span): Bool =>
    (start_index == other.start_index) and
      (end_index == other.end_index) and
      (label is other.label) and
      (discarded == other.discarded)

  fun string(): String =>
    recover val
      String()
        .> append("Span(")
        .> append(start_index.string())
        .> append("..")
        .> append(end_index.string())
        .> append(", ")
        .> append(label.string())
        .> append(if discarded then ", discarded" else "" end)
        .> append(")")
    end
