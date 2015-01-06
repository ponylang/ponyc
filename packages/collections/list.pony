class ListNode[A]
  """
  A node in a list.
  """
  var _item: (A | None)
  var _list: (List[A] | None) = None
  var _prev: (ListNode[A] | None) = None
  var _next: (ListNode[A] | None) = None

  new create(item: A) =>
    """
    Create a node. Initially, it is not in any list.
    """
    _item = consume item

  fun box item(): this->A ? =>
    """
    Return the item, if we have one, otherwise raise an error.
    """
    _item as this->A

  fun ref pop(): A^ ? =>
    """
    Remove the item from the node, if we have one, otherwise raise an error.
    """
    (_item = None) as A^

  fun ref prepend(prev: ListNode[A]): ListNode[A]^ =>
    """
    Prepend a node to this one. If prev is already in a list, it is removed
    before it is prepended.
    """
    if _prev is prev then
      return this
    end

    match _list
    | var list': List[A] =>
      prev.remove()

      match _prev
      | var  prev': ListNode[A] =>
        prev'._next = prev
      else
        list'._set_head(prev)
      end

      prev._prev = _prev
      prev._next = this
      _prev = prev
      list'._increment()
    end
    this

  fun ref append(next: ListNode[A]): ListNode[A]^ =>
    """
    Append a node to this one. If prev is already in a list, it is removed
    before it is appended.
    """
    if _next is next then
      return this
    end

    match _list
    | var list': List[A] =>
      next.remove()

      match _next
      | var  next': ListNode[A] =>
        next'._prev = next
      else
        list'._set_tail(next)
      end

      next._prev = this
      next._next = _next
      _next = next
      list'._increment()
    end
    this

  fun ref remove(): ListNode[A]^ =>
    """
    Remove a node from a list.
    """
    match _list
    | var list': List[A] =>
      match (_prev, _next)
      | (var prev': ListNode[A], var next': ListNode[A]) =>
        // We're in the middle of the list.
        prev'._next = _next
        next'._prev = _prev
        _prev = None
        _next = None
      | (var prev': ListNode[A], None) =>
        // We're the tail.
        prev'._next = None
        list'._set_tail(prev')
        _prev = None
      | (None, var next': ListNode[A]) =>
        // We're the head.
        next'._prev = None
        list'._set_head(next')
        _next = None
      | (None, None) =>
        // We're the only member
        list'._set_head(None)
        list'._set_tail(None)
      end
      list'._decrement()
    end
    this

  fun box prev(): (this->ListNode[A] | None) =>
    """
    Return the previous node.
    """
    _prev

  fun box next(): (this->ListNode[A] | None) =>
    """
    Return the next node.
    """
    _next

class List[A]
  """
  A doubly linked list.
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
      push(consume a)
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
