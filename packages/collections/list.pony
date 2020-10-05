class List[A] is Seq[A]
  """
  A doubly linked list.

  (The following is paraphrased from [Wikipedia](https://en.wikipedia.org/wiki/Doubly_linked_list).)

  A doubly linked list is a linked data structure that consists of a set of sequentially
  linked records called nodes. (Implemented in Ponylang via the collections.ListNode class.) Each
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
        let my_list = List[String]()
        
        env.out.print("A new empty list has " + my_list.size().string() + " nodes.") // 0
        
        // Push a String literal onto our empty List
        my_list.push("A single String")
        env.out.print("Adding one node to our empty list means it now has a size of "
                      + my_list.size().string() + ".") // 1
        
        // Get the first element of our List
        try env.out.print("The first (index 0) node has the value: "
                          + my_list.index(0)?()?.string()) end // A single String
        
        // Create a second List from a single String literal
        let my_second_list = List[String].unit("Another String")
        
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
        let my_third_list = List[String].from(["First"; "Second"; "Third"])
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
  var _head: (ListNode[A] | None) = None
  var _tail: (ListNode[A] | None) = None
  var _size: USize = 0

  new create(len: USize = 0) =>
    """
    Do nothing, but be compatible with Seq.

    Creates an empty list with 0 nodes.

    let my_list = List[String].create()
    """
    None

  new unit(a: A) =>
    """
    Builds a new list from an element.

    let my_list = List[String].unit("element")
    """
    push(consume a)

  new from(seq: Array[A^]) =>
    """
    Builds a new list from the sequence passed in.

    let my_list = List[String].from(["a"; "b"; "c"])
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

    // suppose `my_list` contains ["a"; "b"; "c"]
    env.out.print(my_list.size().string()) // 3
    """
    _size

  fun apply(i: USize = 0): this->A ? =>
    """
    Get the i-th element, raising an error if the index is out of bounds.

    // suppose `my_list` contains ["a"; "b"; "c"]
    try env.out.print(my_list.apply(1)?) end // b
    """
    index(i)?()?

  fun ref update(i: USize, value: A): A^ ? =>
    """
    Change the i-th element, raising an error if the index is out of bounds.
    Returns the previous value, which may be None if the node has been popped
    but left on the list.

    // suppose `my_list` contains ["a"; "b"; "c"]
    try my_list.update(1, "z")? end // ["a"; "z"; "c"]
    """
    index(i)?()? = consume value

  fun index(i: USize): this->ListNode[A] ? =>
    """
    Gets the i-th node, raising an error if the index is out of bounds.

    // suppose `my_list` contains ["a"; "b"; "c"]
    try env.out.print(my_list.index(0)?.apply()?) end // "a"
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

  fun ref remove(i: USize): ListNode[A] ? =>
    """
    Remove the i-th node, raising an error if the index is out of bounds.
    The removed node is returned.

    // suppose `my_list` contains ["a"; "b"; "c"]
    try env.out.print(my_list.remove(0)?.apply()?) end // "a"
    """
    index(i)? .> remove()

  fun ref clear() =>
    """
    Empties the list.
    """
    _head = None
    _tail = None
    _size = 0

  fun head(): this->ListNode[A] ? =>
    """
    Get the head of the list.

    // suppose `my_list` contains ["a"; "b"; "c"]
    try env.out.print(my_list.head()?.apply()?) end // "a"
    """
    _head as this->ListNode[A]

  fun tail(): this->ListNode[A] ? =>
    """
    Get the tail of the list.

    // suppose `my_list` contains ["a"; "b"; "c"]
    try env.out.print(my_list.tail()?.apply()?) end // "c"
    """
    _tail as this->ListNode[A]

  fun ref prepend_node(node: ListNode[A]) =>
    """
    Adds a node to the head of the list.

    // suppose `my_list` contains ["a"; "b"; "c"]
    let new_head = ListNode[String].create("0")
    my_list.prepend_node(new_head) // ["0", "a"; "b"; "c"]
    """
    match _head
    | let head': ListNode[A] =>
      head'.prepend(node)
    else
      _set_both(node)
    end

  fun ref append_node(node: ListNode[A]) =>
    """
    Adds a node to the tail of the list.

    // suppose `my_list` contains ["a"; "b"; "c"]
    let new_tail = ListNode[String].create("0")
    my_list.append_node(new_head) // ["a"; "b"; "c", "0"]
    """
    match _tail
    | let tail': ListNode[A] =>
      tail'.append(node)
    else
      _set_both(node)
    end

  fun ref append_list(that: List[A]) =>
    """
    Remove all nodes from that and append them to this.
    """
    if this isnt that then
      while that._size > 0 do
        try append_node(that.head()?) end
      end
    end

  fun ref prepend_list(that: List[A]) =>
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
    append_node(ListNode[A](consume a))

  fun ref pop(): A^ ? =>
    """
    Removes a value from the tail of the list.
    """
    tail()? .> remove().pop()?

  fun ref unshift(a: A) =>
    """
    Adds a value to the head of the list.
    """
    prepend_node(ListNode[A](consume a))

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
      _map[B](head()?, f, List[B])
    else
      List[B]
    end

  fun _map[B](
    ln: this->ListNode[A],
    f: {(this->A!): B^} box,
    acc: List[B])
    : List[B]^
  =>
    """
    Private helper for map, recursively working with ListNodes.
    """
    try acc.push(f(ln()?)) end

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
      _flat_map[B](head()?, f, List[B])
    else
      List[B]
    end

  fun _flat_map[B](
    ln: this->ListNode[A],
    f: {(this->A!): List[B]} box,
    acc: List[B]): List[B]^
  =>
    """
    Private helper for flat_map, recursively working with ListNodes.
    """
    try acc.append_list(f(ln()?)) end

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
      _filter(head()?, f, List[this->A!])
    else
      List[this->A!]
    end

  fun _filter(
    ln: this->ListNode[A],
    f: {(this->A!): Bool} box,
    acc: List[this->A!]): List[this->A!]
  =>
    """
    Private helper for filter, recursively working with ListNodes.
    """
    try
      let cur = ln()?
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
      head()?
    else
      return acc
    end

    _fold[B](h, f, consume acc)

  fun _fold[B](
    ln: this->ListNode[A],
    f: {(B!, this->A!): B^} box,
    acc: B)
    : B
  =>
    """
    Private helper for fold, recursively working with ListNodes.
    """
    let nextAcc: B = try f(acc, ln()?) else consume acc end
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
      _every(head()?, f)
    else
      true
    end

  fun _every(ln: this->ListNode[A], f: {(this->A!): Bool} box): Bool =>
    """
    Private helper for every, recursively working with ListNodes.
    """
    try
      if not(f(ln()?)) then
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
      _exists(head()?, f)
    else
      false
    end

  fun _exists(ln: this->ListNode[A], f: {(this->A!): Bool} box): Bool =>
    """
    Private helper for exists, recursively working with ListNodes.
    """
    try
      if f(ln()?) then
        true
      else
        _exists(ln.next() as this->ListNode[A], f)
      end
    else
      false
    end

  fun partition(
    f: {(this->A!): Bool} box)
    : (List[this->A!]^, List[this->A!]^)
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
        var node = index(n)?

        for i in Range(n, size()) do
          l.push(node()?)
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
        var node = head()?

        for i in Range(0, n.min(size())) do
          l.push(node()?)
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
        var node = head()?

        for i in Range(0, size()) do
          let item = node()?
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
      _reverse(head()?, List[this->A!])
    else
      List[this->A!]
    end

  fun _reverse(ln: this->ListNode[A], acc: List[this->A!]): List[this->A!]^ =>
    """
    Private helper for reverse, recursively working with ListNodes.
    """
    try acc.unshift(ln()?) end

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
      _contains[B](head()?, a)
    else
      false
    end

  fun _contains[B: (A & HasEq[A!] #read) = A](
    ln: this->ListNode[A],
    a: box->B)
    : Bool
  =>
    """
    Private helper for contains, recursively working with ListNodes.
    """
    try
      if a == ln()? then
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

      next'()?
    else
      error
    end
