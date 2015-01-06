class List[A]
  """
  A doubly linked list.
  """
  var _head: (ListNode[A] | None)
  var _tail: (ListNode[A] | None)
  var _size: U64

  new create(node: (ListNode[A] | None) = None) =>
    """
    Create a new list, optionally starting with an existing node.
    """
    _head = node
    _tail = node

    match node
    | var node': ListNode[A] =>
      node'.remove()
      _size = 1
    else
      _size = 0
    end

  fun box size(): U64 =>
    """
    Returns the number of items in the list.
    """
    _size

  fun box head(): this->ListNode[A] ? =>
    """
    Get the head of the list.
    """
    _head as this->ListNode[A]

  fun box tail(): this->ListNode[A] ? =>
    """
    Get the tail of the list.
    """
    _tail as this->ListNode[A]

  fun ref push(a: A): List[A]^ =>
    """
    Adds a value to the tail of the list.
    """
    match _tail
    | var tail': ListNode[A] =>
      tail'.append(ListNode[A](consume a))
    else
      unshift(consume a)
    end
    this

  fun ref pop(): A^ ? =>
    """
    Removes a value from the tail of the list.
    """
    match _tail
    | var tail': ListNode[A] =>
      tail'.remove().pop()
    else
      error
    end

  fun ref unshift(a: A): List[A]^ =>
    """
    Adds a value to the head of the list.
    """
    let node = ListNode[A](consume a)

    match _head
    | var head': ListNode[A] =>
      head'.prepend(node)
    else
      _head = node
      _tail = node
      _size = 1
    end
    this

  fun ref shift(): A^ ? =>
    """
    Removes a value from the head of the list.
    """
    match _head
    | var head': ListNode[A] =>
      head'.remove().pop()
    else
      error
    end

  fun box values(): ListValues[A, this->ListNode[A]]^ =>
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

  fun box has_next(): Bool =>
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
      next'.item()
    else
      error
    end
