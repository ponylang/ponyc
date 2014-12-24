class ListNode[A]
  """
  A node in a list.
  """
  var _item: (A | None)
  var _next: (ListNode[A] | None)

  new create(item': A, next': (ListNode[A] | None)) =>
    _item = consume item'
    _next = next'

  fun box item(): this->A ? =>
    _item as this->A

  fun ref pop(): A^ ? =>
    (_item = None) as A^

  fun box next(): (this->ListNode[A] | None) =>
    _next

class List[A]
  """
  A singly linked list.
  """
  var _head: (ListNode[A] | None)

  new create() =>
    """
    Creates an empty list.
    """
    _head = None

  fun box size(): U64 =>
    """
    Returns the number of items in the list.
    """
    var len: U64 = 0
    var node = _head

    while true do
      match node
      | var n: this->ListNode[A] =>
        len = len + 1
        node = n.next()
      else
        return len
      end
    end
    len

  fun ref push(a: A): List[A]^ =>
    """
    Pushes a value to the head of the list.
    """
    _head = ListNode[A](consume a, _head)
    this

  fun ref pop(): A^ ? =>
    """
    Pops a value from the head of the list.
    """
    match _head
    | var head: ListNode[A] =>
      _head = head.next()
      head.pop()
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
