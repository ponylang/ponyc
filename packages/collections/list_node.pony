class ListNode[A]
  """
  A node in a doubly linked list.
  
  (See Ponylang [collections.List](https://stdlib.ponylang.io/collections-List/)
  class for usage examples.)
  
  Each node contains four fields: two link fields (references to the previous and
  to the next node in the sequence of nodes), one data field, and the reference to
  the in which it resides.

  As you would expect functions are provided to create a ListNode, update a
  ListNode's contained item, and pop the item from the ListNode.

  Additional functions are provided to operate on a ListNode as part of a Linked
  List. These provide for prepending, appending, removal, and safe traversal in
  both directions.  The Ponylang
  [collections.List](https://stdlib.ponylang.io/collections-List/) class is the
  correct way to create these. _Do not attempt to create a Linked List using only
  ListNodes._
  
  ## Example program
  The functions which are illustrated below are only those which operate on an
  individual ListNode.

  It outputs:

    My node has the item value: My Node item
    My node has the updated item value: My updated Node item
    Popped the item from the ListNode
    The ListNode has no (None) item.

  ```pony
    use "collections"
    actor Main
      new create(env:Env) =>
        
        // Create a new ListNode of type String
        let my_list_node = ListNode[String]("My Node item")
        try 
          env.out.print("My node has the item value: "
                        + my_list_node.apply()?) // My Node item
        end
        
        // Update the item contained in the ListNode
        try
          my_list_node.update("My updated Node item")?
          env.out.print("My node has the updated item value: "
                        + my_list_node.apply()?) // My updated Node item
        end
        // Pop the item from the ListNode
        try
          my_list_node.pop()?
          env.out.print("Popped the item from the ListNode")
          my_list_node.apply()? // This will error as the item is now None
        else
          env.out.print("The ListNode has no (None) item.")
        end
  ...
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

  fun ref prepend(that: ListNode[A]): Bool =>
    """
    Prepend a node to this one. If `that` is already in a list, it is removed
    before it is prepended. Returns true if `that` was removed from another
    list.
    If the ListNode is not contained within a List the prepend will fail.
    """
    if (_prev is that) or (this is that) then
      return false
    end

    var in_list = false

    match _list
    | let list': List[A] =>
      in_list = that._list isnt None
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
    in_list

  fun ref append(that: ListNode[A]): Bool =>
    """
    Append a node to this one. If `that` is already in a list, it is removed
    before it is appended. Returns true if `that` was removed from another
    list.
    
    If the ListNode is not contained within a List the append will fail.
    """
    if (_next is that) or (this is that) then
      return false
    end

    var in_list = false

    match _list
    | let list': List[A] =>
      in_list = that._list isnt None
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
    in_list

  fun ref remove() =>
    """
    Remove a node from a list.
    
    The ListNode must be contained within a List for this to succeed.
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
