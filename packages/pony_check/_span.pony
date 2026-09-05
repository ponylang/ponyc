class val _Span is (Equatable[_Span] & Stringable)
  let start_index: USize
  let end_index: USize
  let label: USize
  let discarded: Bool

  new val create(
    start_index': USize,
    end_index': USize,
    label': USize,
    discarded': Bool = false)
  =>
    start_index = start_index'
    end_index = end_index'
    label = label'
    discarded = discarded'

  fun with_discarded(): _Span =>
    _Span(start_index, end_index, label, true)

  fun eq(other: box->_Span): Bool =>
    (start_index == other.start_index) and
      (end_index == other.end_index) and
      (label == other.label) and
      (discarded == other.discarded)

  fun string(): String iso^ =>
    recover
      String()
        .> append("Span(")
        .> append(start_index.string())
        .> append("..")
        .> append(end_index.string())
        .> append(", label=")
        .> append(label.string())
        .> append(if discarded then ", discarded" else "" end)
        .> append(")")
    end
