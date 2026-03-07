type _Segment is (_ChildSegment | _DescendantSegment)

class val _ChildSegment
  """
  A child segment applies its selectors to each node in the input
  nodelist, collecting all selected values.
  """
  let _selectors: Array[_Selector] val

  new val create(selectors': Array[_Selector] val) =>
    _selectors = selectors'

  fun selectors(): Array[_Selector] val => _selectors

class val _DescendantSegment
  """
  A descendant segment applies its selectors at every level of the
  subtree rooted at each input node (depth-first pre-order per
  RFC 9535 Section 2.5.2).
  """
  let _selectors: Array[_Selector] val

  new val create(selectors': Array[_Selector] val) =>
    _selectors = selectors'

  fun selectors(): Array[_Selector] val => _selectors
