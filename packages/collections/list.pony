class ListNode[A]
  """
  A node in a list.
  """
  var _item: (A | None)
  var _next: (ListNode[A] | None)

  new create(item': A, next': (ListNode[A] | None) = None) =>
    _item = consume item'
    _next = next'

  fun box item(): this->A ? =>
    _item as this->A

  fun ref pop(): A^ ? =>
    (_item = None) as A^

  fun ref append(next: ListNode[A]): ListNode[A]^ =>
    _next = next
    this

  fun box next(): (this->ListNode[A] | None) =>
    _next

class List[A]
  """
  A singly linked list.
  """
  var _head: (ListNode[A] | None) = None
  var _tail: (ListNode[A] | None) = None
  var _size: U64 = 0

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

  fun ref append(a: A): List[A]^ =>
    """
    Appends a value to the tail of the list.
    """
    match _tail
    | var tail: ListNode[A] =>
      var next = ListNode[A](consume a)
      tail.append(next)
      _tail = next
      _size = _size + 1
    else
      push(consume a)
    end
    this

  fun ref push(a: A): List[A]^ =>
    """
    Pushes a value to the head of the list.
    """
    _head = ListNode[A](consume a, _head)

    if _size == 0 then
      _tail = _head
    end

    _size = _size + 1
    this

  fun ref pop(): A^ ? =>
    """
    Pops a value from the head of the list.
    """
    match _head
    | var head': ListNode[A] =>
      _size = _size - 1
      _head = head'.next()

      if _size == 0 then
        _tail = None
      end

      head'.pop()
    else
      error
    end

  fun box values(): ListValues[A, this->ListNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListValues[A, this->ListNode[A]](_head)

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
