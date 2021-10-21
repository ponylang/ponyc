use "itertools"

class ListIter[A] is Seq[A]
  """
  A doubly linked list.

  (The following is paraphrased from [Wikipedia](https://en.wikipedia.org/wiki/Doubly_linked_list).)

  A doubly linked list is a linked data structure that consists of a set of sequentially
  linked records called nodes. (Implemented in Ponylang via the collections.ListIterNode class.) Each
  node contains four fields: two link fields (references to the previous and to the next node in
  the sequence of nodes), one data field, and the reference to the in which it resides.  A doubly
  linked list can be conceptualized as two singly linked lists formed from the same data items, but
  in opposite sequential orders.

  As you would expect. functions are provided to perform all the common list operations such as
  creation, traversal, node addition and removal, iteration, mapping, filtering, etc.

  ## Example program
  There are a _lot_ of functions in List. The following code picks out a few common examples.

  It outputs:

      A new empty list has 0 nodes.
      Adding one node to our empty list means it now has a size of 1.
      The first (index 0) node has the value: A single String
      A list created by appending our second single-node list onto our first has size: 2
      The List nodes of our first list are now:
        A single String
        Another String
      Append *moves* the nodes from the second list so that now has 0 nodes.
      A list created from an array of three strings has size: 3
        First
        Second
        Third
      Mapping over our three-node list produces a new list of size: 3
      Each node-value in the resulting list is now far more exciting:
        First BOOM!
        Second BOOM!
        Third BOOM!
      Filtering our three-node list produces a new list of size: 2
        Second BOOM!
        Third BOOM!
      The size of our first partitioned list (matches predicate): 1
      The size of our second partitioned list (doesn't match predicate): 1
      Our matching partition elements are:
        Second BOOM!

  ```pony
    use "collections"

    actor Main
      new create(env:Env) =>

        // Create a new empty List of type String
        let my_list = ListIter[String]()

        env.out.print("A new empty list has " + my_list.size().string() + " nodes.") // 0

        // Push a String literal onto our empty List
        my_list.push("A single String")
        env.out.print("Adding one node to our empty list means it now has a size of "
                      + my_list.size().string() + ".") // 1

        // Get the first element of our List
        try env.out.print("The first (index 0) node has the value: "
                          + my_list.index(0)?()?.string()) end // A single String

        // Create a second List from a single String literal
        let my_second_list = ListIter[String].unit("Another String")

        // Append the second List to the first
        my_list.append_list(my_second_list)
        env.out.print("A list created by appending our second single-node list onto our first has size: "
                      + my_list.size().string()) // 2
        env.out.print("The List nodes of our first list are now:")
        for n in my_list.values() do
          env.out.print("\t" + n.string())
        end
        // NOTE: this _moves_ the elements so second_list consequently ends up empty
        env.out.print("Append *moves* the nodes from the second list so that now has "
                      + my_second_list.size().string() + " nodes.") // 0

        // Create a third List from a Seq(ence)
        // (In this case a literal array of Strings)
        let my_third_list = ListIter[String].from(["First"; "Second"; "Third"])
        env.out.print("A list created from an array of three strings has size: "
                      + my_third_list.size().string()) // 3
        for n in my_third_list.values() do
          env.out.print("\t" + n.string())
        end

        // Map over the third List, concatenating some "BOOM!'s" into a new List
        let new_list = my_third_list.map[String]({ (n) => n + " BOOM!" })
        env.out.print("Mapping over our three-node list produces a new list of size: "
                      + new_list.size().string()) // 3
        env.out.print("Each node-value in the resulting list is now far more exciting:")
        for n in new_list.values() do
          env.out.print("\t" + n.string())
        end

        // Filter the new list to extract 2 elements
        let filtered_list = new_list.filter({ (n) => n.string().contains("d BOOM!") })
        env.out.print("Filtering our three-node list produces a new list of size: "
                          + filtered_list.size().string()) // 2
        for n in filtered_list.values() do
          env.out.print("\t" + n.string()) // Second BOOM!\nThird BOOM!
        end

        // Partition the filtered list
        let partitioned_lists = filtered_list.partition({ (n) => n.string().contains("Second") })
        env.out.print("The size of our first partitioned list (matches predicate): " + partitioned_lists._1.size().string())        // 1
        env.out.print("The size of our second partitioned list (doesn't match predicate): " + partitioned_lists._2.size().string())  // 1
        env.out.print("Our matching partition elements are:")
        for n in partitioned_lists._1.values() do
          env.out.print("\t" + n.string()) // Second BOOM!
        end
  ```

  """
  var _head: (ListIterNode[A] | None) = None
  var _tail: (ListIterNode[A] | None) = None
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

  fun ref reserve(len: USize) =>
    """
    Do nothing, but be compatible with Seq.
    """
    None

  fun size(): USize =>
    """
    Returns the number of items in the list.
    """
    _size

  fun apply(i: USize = 0): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.
    """
    index(i)?()?

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    Returns the previous value, which may be None if the node has been popped
    but left on the list.
    """
    index(i)?()? = consume value

  fun index(i: USize): this->ListIterNode[A] ? =>
    """
    Gets the i-th node, raising an error if the index is out of bounds.
    """
    if i >= _size then
      error
    end

    var node = _head as this->ListIterNode[A]
    var j = USize(0)

    while j < i do
      node = node.next() as this->ListIterNode[A]
      j = j + 1
    end

    node

  fun ref remove(i: USize): ListIterNode[A] ? =>
    """
    Remove the i-th node, raising an error if the index is out of bounds.
    The removed node is returned.
    """
    index(i)? .> remove()

  fun ref clear() =>
    """
    Empties the list.
    """
    _head = None
    _tail = None
    _size = 0

  fun head(): this->ListIterNode[A] ? =>
    """
    Get the head of the list.
    """
    _head as this->ListIterNode[A]

  fun tail(): this->ListIterNode[A] ? =>
    """
    Get the tail of the list.
    """
    _tail as this->ListIterNode[A]

  fun ref prepend_node(node: ListIterNode[A]) =>
    """
    Adds a node to the head of the list.
    """
    match _head
    | let head': ListIterNode[A] =>
      head'.prepend(node)
    else
      _set_both(node)
    end

  fun ref append_node(node: ListIterNode[A]) =>
    """
    Adds a node to the tail of the list.
    """
    match _tail
    | let tail': ListIterNode[A] =>
      tail'.append(node)
    else
      _set_both(node)
    end

  fun ref append_list(that: ListIter[A]) =>
    """
    Remove all nodes from that and append them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try append_node(that.head()?) end
      end
    end

  fun ref prepend_list(that: ListIter[A]) =>
    """
    Remove all nodes from that and prepend them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try prepend_node(that.tail()?) end
      end
    end

  fun ref push(a: A) =>
    """
    Adds a value to the tail of the list.
    """
    append_node(ListIterNode[A](consume a))

  fun ref pop(): A^ ? =>
    """
    Removes a value from the tail of the list.
    """
    tail()? .> remove().pop()?

  fun ref unshift(a: A) =>
    """
    Adds a value to the head of the list.
    """
    prepend_node(ListIterNode[A](consume a))

  fun ref shift(): A^ ? =>
    """
    Removes a value from the head of the list.
    """
    head()? .> remove().pop()?

  fun ref append(
    seq: (ReadSeq[A] & ReadElement[A^]),
    offset: USize = 0,
    len: USize = -1)
  =>
    """
    Append len elements from a sequence, starting from the given offset.
    """
    if offset >= seq.size() then
      return
    end

    let copy_len = len.min(seq.size() - offset)
    reserve(_size + copy_len)

    let cap = copy_len + offset
    var i = offset

    try
      while i < cap do
        push(seq(i)?)
        i = i + 1
      end
    end

  fun ref concat(iter: Iterator[A^], offset: USize = 0, len: USize = -1) =>
    """
    Add len iterated elements to the end of the list, starting from the given
    offset.
    """
    try
      for i in Range(0, offset) do
        if iter.has_next() then
          iter.next()?
        else
          return
        end
      end

      for i in Range(0, len) do
        if iter.has_next() then
          push(iter.next()?)
        else
          return
        end
      end
    end

  fun ref truncate(len: USize) =>
    """
    Truncate the list to the given length, discarding excess elements.
    If the list is already smaller than len, do nothing.
    """
    try
      while _size > len do
        pop()?
      end
    end

  fun clone(): ListIter[this->A!]^ =>
    """
    Clone the list.
    """
    let out = ListIter[this->A!]

    for v in values() do
      out.push(v)
    end
    out

  fun map[B](f: {(this->A!): B^} box): ListIter[B]^ =>
    """
    Builds a new list by applying a function to every member of the list.
    """
    Iter[this->A!](this.values()).map[B^](f).collect[ListIter[B]^](ListIter[B](this.size()))

  fun flat_map[B](f: {(this->A!): ListIter[B]} box): ListIter[B]^ =>
    """
    Builds a new list by applying a function to every member of the list and
    using the elements of the resulting lists.
    """
    try
      _flat_map[B](head()?, f, ListIter[B])
    else
      ListIter[B]
    end

  fun _flat_map[B](
    ln: this->ListIterNode[A],
    f: {(this->A!): ListIter[B]} box,
    acc: ListIter[B]): ListIter[B]^
  =>
    """
    Private helper for flat_map, recursively working with ListNodes.
    """
    try acc.append_list(f(ln()?)) end

    try
      _flat_map[B](ln.next() as this->ListIterNode[A], f, acc)
    else
      acc
    end

  fun filter(f: {(this->A!): Bool} box): ListIter[this->A!]^ =>
    """
    Builds a new list with those elements that satisfy a provided predicate.
    """
    Iter[this->A!](this.values()).filter(f).collect[ListIter[this->A!]^](ListIter[this->A!])

  fun fold[B](acc: B^, f: {(B!, this->A!): B^} box): B =>
    """
    Folds the elements of the list using the supplied function.
    """
    Iter[this->A!](this.values()).fold[B](acc, f)

  fun every(f: {(this->A!): Bool} box): Bool =>
    """
    Returns true if every element satisfies the provided predicate, false
    otherwise.
    """
    Iter[this->A!](this.values()).all(f)

  fun exists(f: {(this->A!): Bool} box): Bool =>
    """
    Returns true if at least one element satisfies the provided predicate,
    false otherwise.
    """
    Iter[this->A!](this.values()).any(f)

  fun partition(
    f: {(this->A!): Bool} box)
    : (ListIter[this->A!]^, ListIter[this->A!]^)
  =>
    """
    Builds a pair of lists, the first of which is made up of the elements
    satisfying the supplied predicate and the second of which is made up of
    those that do not.
    """
    let l1 = ListIter[this->A!]
    let l2 = ListIter[this->A!]
    for item in values() do
      if f(item) then l1.push(item) else l2.push(item) end
    end
    (l1, l2)

  fun drop(n: USize): ListIter[this->A!]^ =>
    """
    Builds a list by dropping the first n elements.
    """
    Iter[this->A!](this.values()).skip(n).collect[ListIter[this->A!]^](ListIter[this->A!])

  fun take(n: USize): ListIter[this->A!] =>
    """
    Builds a list of the first n elements.
    """
    Iter[this->A!](this.values()).take(n).collect[ListIter[this->A!]^](ListIter[this->A!])

  fun take_while(f: {(this->A!): Bool} box): ListIter[this->A!]^ =>
    """
    Builds a list of elements satisfying the provided predicate until one does
    not.
    """
    Iter[this->A!](this.values()).take_while(f).collect[ListIter[this->A!]^](ListIter[this->A!])

  fun reverse(): ListIter[this->A!]^ =>
    """
    Builds a new list by reversing the elements in the list.
    """
    try
      _reverse(head()?, ListIter[this->A!])
    else
      ListIter[this->A!]
    end

  fun _reverse(ln: this->ListIterNode[A], acc: ListIter[this->A!]): ListIter[this->A!]^ =>
    """
    Private helper for reverse, recursively working with ListIterNodes.
    """
    try acc.unshift(ln()?) end

    try
      _reverse(ln.next() as this->ListIterNode[A], acc)
    else
      acc
    end

  fun contains[B: (A & HasEq[A!] #read) = A](a: box->B): Bool =>
    """
    Returns true if the list contains the provided element, false otherwise.
    """
    try
      _contains[B](head()?, a)
    else
      false
    end

  fun _contains[B: (A & HasEq[A!] #read) = A](
    ln: this->ListIterNode[A],
    a: box->B)
    : Bool
  =>
    """
    Private helper for contains, recursively working with ListIterNodes.
    """
    try
      if a == ln()? then
        true
      else
        _contains[B](ln.next() as this->ListIterNode[A], a)
      end
    else
      false
    end

  fun nodes(): ListIterNodes[A, this->ListIterNode[A]]^ =>
    """
    Return an iterator on the nodes in the list.
    """
    ListIterNodes[A, this->ListIterNode[A]](_head)

  fun rnodes(): ListIterNodes[A, this->ListIterNode[A]]^ =>
    """
    Return an iterator on the nodes in the list.
    """
    ListIterNodes[A, this->ListIterNode[A]](_head, true)

  fun values(): ListIterValues[A, this->ListIterNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListIterValues[A, this->ListIterNode[A]](_head)

  fun rvalues(): ListIterValues[A, this->ListIterNode[A]]^ =>
    """
    Return an iterator on the values in the list.
    """
    ListIterValues[A, this->ListIterNode[A]](_head, true)

  fun ref _increment() =>
    _size = _size + 1

  fun ref _decrement() =>
    _size = _size - 1

  fun ref _set_head(head': (ListIterNode[A] | None)) =>
    _head = head'

  fun ref _set_tail(tail': (ListIterNode[A] | None)) =>
    _tail = tail'

  fun ref _set_both(node: ListIterNode[A]) =>
    node._set_list(this)
    _head = node
    _tail = node
    _size = 1


class ListIterNode[A]
  """
  A node in a doubly linked list.

  (See Ponylang [collections.List](https://stdlib.ponylang.io/collections-List/)
  class for usage examples.)

  Each node contains four fields: two link fields (references to the previous and
  to the next node in the sequence of nodes), one data field, and the reference to
  the in which it resides.

  As you would expect functions are provided to create a ListIterNode, update a
  ListIterNode's contained item, and pop the item from the ListIterNode.

  Additional functions are provided to operate on a ListIterNode as part of a Linked
  List. These provide for prepending, appending, removal, and safe traversal in
  both directions.  The Ponylang
  [collections.List](https://stdlib.ponylang.io/collections-List/) class is the
  correct way to create these. _Do not attempt to create a Linked List using only
  ListIterNodes._

  ## Example program
  The functions which are illustrated below are only those which operate on an
  individual ListIterNode.

  It outputs:

    My node has the item value: My Node item
    My node has the updated item value: My updated Node item
    Popped the item from the ListIterNode
    The ListIterNode has no (None) item.

  ```pony
    use "collections"
    actor Main
      new create(env:Env) =>

        // Create a new ListIterNode of type String
        let my_list_node = ListIterNode[String]("My Node item")
        try 
          env.out.print("My node has the item value: "
                        + my_list_node.apply()?) // My Node item
        end

        // Update the item contained in the ListIterNode
        try
          my_list_node.update("My updated Node item")?
          env.out.print("My node has the updated item value: "
                        + my_list_node.apply()?) // My updated Node item
        end
        // Pop the item from the ListIterNode
        try
          my_list_node.pop()?
          env.out.print("Popped the item from the ListIterNode")
          my_list_node.apply()? // This will error as the item is now None
        else
          env.out.print("The ListIterNode has no (None) item.")
        end
  ...
  """
  var _item: (A | None)
  var _list: (ListIter[A] | None) = None
  var _prev: (ListIterNode[A] | None) = None
  var _next: (ListIterNode[A] | None) = None

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

  fun ref prepend(that: ListIterNode[A]): Bool =>
    """
    Prepend a node to this one. If `that` is already in a list, it is removed
    before it is prepended. Returns true if `that` was removed from another
    list.
    If the ListIterNode is not contained within a List the prepend will fail.
    """
    if (_prev is that) or (this is that) then
      return false
    end

    var in_list = false

    match _list
    | let list': ListIter[A] =>
      in_list = that._list isnt None
      that.remove()

      match _prev
      | let  prev': ListIterNode[A] =>
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

  fun ref append(that: ListIterNode[A]): Bool =>
    """
    Append a node to this one. If `that` is already in a list, it is removed
    before it is appended. Returns true if `that` was removed from another
    list.

    If the ListIterNode is not contained within a List the append will fail.
    """
    if (_next is that) or (this is that) then
      return false
    end

    var in_list = false

    match _list
    | let list': ListIter[A] =>
      in_list = that._list isnt None
      that.remove()

      match _next
      | let  next': ListIterNode[A] =>
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

    The ListIterNode must be contained within a List for this to succeed.
    """
    match _list
    | let list': ListIter[A] =>
      match (_prev, _next)
      | (let prev': ListIterNode[A], let next': ListIterNode[A]) =>
        // We're in the middle of the list.
        prev'._next = _next
        next'._prev = _prev
        _prev = None
        _next = None
      | (let prev': ListIterNode[A], None) =>
        // We're the tail.
        prev'._next = None
        list'._set_tail(prev')
        _prev = None
      | (None, let next': ListIterNode[A]) =>
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

  fun prev(): (this->ListIterNode[A] | None) =>
    """
    Return the previous node.
    """
    _prev

  fun next(): (this->ListIterNode[A] | None) =>
    """
    Return the next node.
    """
    _next

  fun ref _set_list(list: ListIter[A]): ListIterNode[A]^ =>
    """
    Make this node the only node on the given list.
    """
    remove()
    _list = list
    this

class ListIterNodes[A, N: ListIterNode[A] #read] is Iterator[N]
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

class ListIterValues[A, N: ListIterNode[A] #read] is Iterator[N->A]
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

      next'()?
    else
      error
    end
