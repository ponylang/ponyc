class List[A] is Seq[A]
  """
  A doubly linked list.
  """
  var _head: (ListNode[A] | None) = None
  var _tail: (ListNode[A] | None) = None
  var _size: USize = 0

  new create(len: USize = 0) =>
    """
    Do nothing, but be compatible with Seq.
    """
    None

  new unit(a: A) =>
    """
    Builds a new list from an element.
    """
    push(consume a)

  new from(seq: Array[A^]) =>
    """
    Builds a new list from the sequence passed in.
    """
    for value in seq.values() do
      push(consume value)
    end

  fun ref reserve(len: USize): List[A]^ =>
    """
    Do nothing, but be compatible with Seq.
    """
    this

  fun size(): USize =>
    """
    Returns the number of items in the list.
    """
    _size

  fun apply(i: USize = 0): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    index(i)()

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    Returns the previous value, which may be None if the node has been popped
    but left on the list.
    """
    index(i)() = consume value

  fun index(i: USize): this->ListNode[A] ? =>
    """
    Gets the i-th node, raising an error if the index is out of bounds.
    """
    if i >= _size then
      error
    end

    var node = _head as this->ListNode[A]
    var j = USize(0)

    while j < i do
      node = node.next() as this->ListNode[A]
      j = j + 1
    end

    node

  fun ref remove(i: USize): List[A]^ ? =>
    """
    Remove the i-th node, raising an error if the index is out of bounds.
    """
    index(i).remove()
    this

  fun ref clear(): List[A]^ =>
    """
    Empties the list.
    """
    _head = None
    _tail = None
    _size = 0
    this

  fun head(): this->ListNode[A] ? =>
    """
    Get the head of the list.
    """
    _head as this->ListNode[A]

  fun tail(): this->ListNode[A] ? =>
    """
    Get the tail of the list.
    """
    _tail as this->ListNode[A]

  fun ref prepend_node(node: ListNode[A]): List[A]^ =>
    """
    Adds a node to the head of the list.
    """
    match _head
    | let head': ListNode[A] =>
      head'.prepend(node)
    else
      _set_both(node)
    end
    this

  fun ref append_node(node: ListNode[A]): List[A]^ =>
    """
    Adds a node to the tail of the list.
    """
    match _tail
    | let tail': ListNode[A] =>
      tail'.append(node)
    else
      _set_both(node)
    end
    this

  fun ref append_list(that: List[A]): List[A]^ =>
    """
    Remove all nodes from that and append them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try append_node(that.head()) end
      end
    end
    this

  fun ref prepend_list(that: List[A]): List[A]^ =>
    """
    Remove all nodes from that and prepend them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try prepend_node(that.tail()) end
      end
    end
    this

  fun ref push(a: A): List[A]^ =>
    """
    Adds a value to the tail of the list.
    """
    append_node(ListNode[A](consume a))

  fun ref pop(): A^ ? =>
    """
    Removes a value from the tail of the list.
    """
    tail().remove().pop()

  fun ref unshift(a: A): List[A]^ =>
    """
    Adds a value to the head of the list.
    """
    prepend_node(ListNode[A](consume a))

  fun ref shift(): A^ ? =>
    """
    Removes a value from the head of the list.
    """
    head().remove().pop()

  fun ref append(seq: (ReadSeq[A] & ReadElement[A^]), offset: USize = 0,
    len: USize = -1): List[A]^
  =>
    """
    Append len elements from a sequence, starting from the given offset.
    """
    if offset >= seq.size() then
      return this
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    let cap = copy_len + offset
    var i = offset

    try
      while i < cap do
        push(seq(i))
        i = i + 1
      end
    end

    this

  fun ref concat(iter: Iterator[A^], offset: USize = 0, len: USize = -1)
    : List[A]^
  =>
    """
    Add len iterated elements to the end of the list, starting from the given
    offset.
    """
    try
      for i in Range(0, offset) do
        if iter.has_next() then
          iter.next()
        else
          return this
        end
      end

      for i in Range(0, len) do
        if iter.has_next() then
          push(iter.next())
        else
          return this
        end
      end
    end

    this

  fun ref truncate(len: USize): List[A]^ =>
    """
    Truncate the list to the given length, discarding excess elements.
    If the list is already smaller than len, do nothing.
    """
    try
      while _size > len do
        pop()
      end
    end

    this

  fun clone(): List[this->A!]^ =>
    """
    Clone the list.
    """
    let out = List[this->A!]

    for v in values() do
      out.push(v)
    end
    out

  fun map[B](f: {(this->A!): B^} box): List[B]^ =>
    """
    Builds a new list by applying a function to every member of the list.
    """
    try
      _map[B](head(), f, List[B])
    else
      List[B]
    end

  fun _map[B](ln: this->ListNode[A], f: {(this->A!): B^} box, acc: List[B])
    : List[B]^
  =>
    """
    Private helper for map, recursively working with ListNodes.
    """
    try acc.push(f(ln())) end

    try
      _map[B](ln.next() as this->ListNode[A], f, acc)
    else
      acc
    end

  fun flat_map[B](f: {(this->A!): List[B]} box): List[B]^ =>
    """
    Builds a new list by applying a function to every member of the list and
    using the elements of the resulting lists.
    """
    try
      _flat_map[B](head(), f, List[B])
    else
      List[B]
    end

  fun _flat_map[B](ln: this->ListNode[A], f: {(this->A!): List[B]} box,
    acc: List[B]): List[B]^
  =>
    """
    Private helper for flat_map, recursively working with ListNodes.
    """
    try acc.append_list(f(ln())) end

    try
      _flat_map[B](ln.next() as this->ListNode[A], f, acc)
    else
      acc
    end

  fun filter(f: {(this->A!): Bool} box): List[this->A!]^ =>
    """
    Builds a new list with those elements that satisfy a provided predicate.
    """
    try
      _filter(head(), f, List[this->A!])
    else
      List[this->A!]
    end

  fun _filter(ln: this->ListNode[A], f: {(this->A!): Bool} box,
    acc: List[this->A!]): List[this->A!]
  =>
    """
    Private helper for filter, recursively working with ListNodes.
    """
    try
      let cur = ln()
      if f(cur) then acc.push(cur) end
    end

    try
      _filter(ln.next() as this->ListNode[A], f, acc)
    else
      acc
    end

  fun fold[B](f: {(B!, this->A!): B^} box, acc: B): B =>
    """
    Folds the elements of the list using the supplied function.
    """
    let h = try
      head()
    else
      return acc
    end

    _fold[B](h, f, consume acc)

  fun _fold[B](ln: this->ListNode[A], f: {(B!, this->A!): B^} box, acc: B): B
  =>
    """
    Private helper for fold, recursively working with ListNodes.
    """
    let nextAcc: B = try f(acc, ln()) else consume acc end
    let h = try
      ln.next() as this->ListNode[A]
    else
      return nextAcc
    end

    _fold[B](h, f, consume nextAcc)

  fun every(f: {(this->A!): Bool} box): Bool =>
    """
    Returns true if every element satisfies the provided predicate, false
    otherwise.
    """
    try
      _every(head(), f)
    else
      true
    end

  fun _every(ln: this->ListNode[A], f: {(this->A!): Bool} box): Bool =>
    """
    Private helper for every, recursively working with ListNodes.
    """
    try
      if not(f(ln())) then
        false
      else
        _every(ln.next() as this->ListNode[A], f)
      end
    else
      true
    end

  fun exists(f: {(this->A!): Bool} box): Bool =>
    """
    Returns true if at least one element satisfies the provided predicate,
    false otherwise.
    """
    try
      _exists(head(), f)
    else
      false
    end

  fun _exists(ln: this->ListNode[A], f: {(this->A!): Bool} box): Bool =>
    """
    Private helper for exists, recursively working with ListNodes.
    """
    try
      if f(ln()) then
        true
      else
        _exists(ln.next() as this->ListNode[A], f)
      end
    else
      false
    end

  fun partition(f: {(this->A!): Bool} box): (List[this->A!]^, List[this->A!]^)
  =>
    """
    Builds a pair of lists, the first of which is made up of the elements
    satisfying the supplied predicate and the second of which is made up of
    those that do not.
    """
    let l1 = List[this->A!]
    let l2 = List[this->A!]
    for item in values() do
      if f(item) then l1.push(item) else l2.push(item) end
    end
    (l1, l2)

  fun drop(n: USize): List[this->A!]^ =>
    """
    Builds a list by dropping the first n elements.
    """
    let l = List[this->A!]

    if size() > n then
      try
        var node = index(n)

        for i in Range(n, size()) do
          l.push(node())
          node = node.next() as this->ListNode[A]
        end
      end
    end
    l

  fun take(n: USize): List[this->A!] =>
    """
    Builds a list of the first n elements.
    """
    let l = List[this->A!]

    if size() > 0 then
      try
        var node = head()

        for i in Range(0, n.min(size())) do
          l.push(node())
          node = node.next() as this->ListNode[A]
        end
      end
    end
    l

  fun take_while(f: {(this->A!): Bool} box): List[this->A!]^ =>
    """
    Builds a list of elements satisfying the provided predicate until one does
    not.
    """
    let l = List[this->A!]

    if size() > 0 then
      try
        var node = head()

        for i in Range(0, size()) do
          let item = node()
          if f(item) then l.push(item) else return l end
          node = node.next() as this->ListNode[A]
        end
      end
    end
    l

  fun reverse(): List[this->A!]^ =>
    """
    Builds a new list by reversing the elements in the list.
    """
    try
      _reverse(head(), List[this->A!])
    else
      List[this->A!]
    end

  fun _reverse(ln: this->ListNode[A], acc: List[this->A!]): List[this->A!]^ =>
    """
    Private helper for reverse, recursively working with ListNodes.
    """
    try acc.unshift(ln()) end

    try
      _reverse(ln.next() as this->ListNode[A], acc)
    else
      acc
    end

  fun contains[B: (A & HasEq[A!] #read) = A](a: box->B): Bool =>
    """
    Returns true if the list contains the provided element, false otherwise.
    """
    try
      _contains[B](head(), a)
    else
      false
    end

  fun _contains[B: (A & HasEq[A!] #read) = A](ln: this->ListNode[A],
    a: box->B): Bool
  =>
    """
    Private helper for contains, recursively working with ListNodes.
    """
    try
      if a == ln() then
        true
      else
        _contains[B](ln.next() as this->ListNode[A], a)
      end
    else
      false
    end

  fun nodes(): ListNodes[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the nodes in the list.
    """
    ListNodes[A, this->ListNode[A]](_head)

  fun rnodes(): ListNodes[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the nodes in the list.
    """
    ListNodes[A, this->ListNode[A]](_head, true)

  fun values(): ListValues[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListValues[A, this->ListNode[A]](_head)

  fun rvalues(): ListValues[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListValues[A, this->ListNode[A]](_head, true)

  fun ref _increment() =>
    _size = _size + 1

  fun ref _decrement() =>
    _size = _size - 1

  fun ref _set_head(head': (ListNode[A] | None)) =>
    _head = head'

  fun ref _set_tail(tail': (ListNode[A] | None)) =>
    _tail = tail'

  fun ref _set_both(node: ListNode[A]) =>
    node._set_list(this)
    _head = node
    _tail = node
    _size = 1

class ListNodes[A, N: ListNode[A] #read] is Iterator[N]
  """
  Iterate over the nodes in a list.
  """
  var _next: (N | None)
  let _reverse: Bool

  new create(head: (N | None), reverse: Bool = false) =>
    """
    Keep the next list node to be examined.
    """
    _next = head
    _reverse = reverse

  fun has_next(): Bool =>
    """
    If we have a list node, we have more values.
    """
    _next isnt None

  fun ref next(): N ? =>
    """
    Get the list node and replace it with the next one.
    """
    match _next
    | let next': N =>
      if _reverse then
        _next = next'.prev()
      else
        _next = next'.next()
      end

      next'
    else
      error
    end

class ListValues[A, N: ListNode[A] #read] is Iterator[N->A]
  """
  Iterate over the values in a list.
  """
  var _next: (N | None)
  let _reverse: Bool

  new create(head: (N | None), reverse: Bool = false) =>
    """
    Keep the next list node to be examined.
    """
    _next = head
    _reverse = reverse

  fun has_next(): Bool =>
    """
    If we have a list node, we have more values.
    """
    _next isnt None

  fun ref next(): N->A ? =>
    """
    Get the value of the list node and replace it with the next one.
    """
    match _next
    | let next': N =>
      if _reverse then
        _next = next'.prev()
      else
        _next = next'.next()
      end

      next'()
    else
      error
    end
