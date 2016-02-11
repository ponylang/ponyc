class ListNode[A]
  """
  A node in a list.
  """
  var _item: (A | None)
  var _list: (List[A] | None) = None
  var _prev: (ListNode[A] | None) = None
  var _next: (ListNode[A] | None) = None

  new create(item: (A | None) = None) =>
    """
    Create a node. Initially, it is not in any list.
    """
    _item = consume item

  fun apply(): this->A ? =>
    """
    Return the item, if we have one, otherwise raise an error.
    """
    _item as this->A

  fun ref update(value: (A | None)): A^ ? =>
    """
    Replace the item and return the previous one. Raise an error if we have no
    previous value.
    """
    (_item = consume value) as A^

  fun ref pop(): A^ ? =>
    """
    Remove the item from the node, if we have one, otherwise raise an error.
    """
    (_item = None) as A^

  fun ref prepend(that: ListNode[A]): ListNode[A]^ =>
    """
    Prepend a node to this one. If `that` is already in a list, it is removed
    before it is prepended.
    """
    if (_prev is that) or (this is that) then
      return this
    end

    match _list
    | let list': List[A] =>
      that.remove()

      match _prev
      | let  prev': ListNode[A] =>
        prev'._next = that
      else
        list'._set_head(that)
      end

      that._list = list'
      that._prev = _prev
      that._next = this
      _prev = that
      list'._increment()
    end
    this

  fun ref append(that: ListNode[A]): ListNode[A]^ =>
    """
    Append a node to this one. If `that` is already in a list, it is removed
    before it is appended.
    """
    if (_next is that) or (this is that) then
      return this
    end

    match _list
    | let list': List[A] =>
      that.remove()

      match _next
      | let  next': ListNode[A] =>
        next'._prev = that
      else
        list'._set_tail(that)
      end

      that._list = list'
      that._prev = this
      that._next = _next
      _next = that
      list'._increment()
    end
    this

  fun ref remove(): ListNode[A]^ =>
    """
    Remove a node from a list.
    """
    match _list
    | let list': List[A] =>
      match (_prev, _next)
      | (let prev': ListNode[A], let next': ListNode[A]) =>
        // We're in the middle of the list.
        prev'._next = _next
        next'._prev = _prev
        _prev = None
        _next = None
      | (let prev': ListNode[A], None) =>
        // We're the tail.
        prev'._next = None
        list'._set_tail(prev')
        _prev = None
      | (None, let next': ListNode[A]) =>
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
      _list = None
    end
    this

  fun has_prev(): Bool =>
    """
    Return true if there is a previous node.
    """
    _prev isnt None

  fun has_next(): Bool =>
    """
    Return true if there is a next node.
    """
    _next isnt None

  fun prev(): (this->ListNode[A] | None) =>
    """
    Return the previous node.
    """
    _prev

  fun next(): (this->ListNode[A] | None) =>
    """
    Return the next node.
    """
    _next

  fun ref _set_list(list: List[A]): ListNode[A]^ =>
    """
    Make this node the only node on the given list.
    """
    remove()
    _list = list
    this
