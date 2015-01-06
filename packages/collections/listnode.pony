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
