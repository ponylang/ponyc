class List[A]
  """
  A doubly linked list.
  """
  var _head: (ListNode[A] | None) = None
  var _tail: (ListNode[A] | None) = None
  var _size: U64 = 0

  fun size(): U64 =>
    """
    Returns the number of items in the list.
    """
    _size

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

  fun ref prepend(node: ListNode[A]): List[A]^ =>
    """
    Adds a node to the head of the list.
    """
    match _head
    | var head': ListNode[A] =>
      head'.prepend(node)
    else
      _set_both(node)
    end
    this

  fun ref append(node: ListNode[A]): List[A]^ =>
    """
    Adds a node to the tail of the list.
    """
    match _tail
    | var tail': ListNode[A] =>
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
        try append(that.head()) end
      end
    end
    this

  fun ref prepend_list(that: List[A]): List[A]^ =>
    """
    Remove all nodes from that and prepend them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try prepend(that.tail()) end
      end
    end
    this

  fun ref push(a: A): List[A]^ =>
    """
    Adds a value to the tail of the list.
    """
    append(ListNode[A](consume a))

  fun ref pop(): A^ ? =>
    """
    Removes a value from the tail of the list.
    """
    tail().remove().pop()

  fun ref unshift(a: A): List[A]^ =>
    """
    Adds a value to the head of the list.
    """
    prepend(ListNode[A](consume a))

  fun ref shift(): A^ ? =>
    """
    Removes a value from the head of the list.
    """
    head().remove().pop()

  fun values(): ListValues[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListValues[A, this->ListNode[A]](_head)

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

class ListValues[A, N: ListNode[A] box] is Iterator[N->A]
  """
  Iterate over the values in a list.
  """
  var _next: (N | None)

  new create(head: (N | None)) =>
    """
    Keep the next list node to be examined.
    """
    _next = head

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
      _next = next'.next()
      next'()
    else
      error
    end
