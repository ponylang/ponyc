primitive Lists
  """
  Helper functions for List. All functions are non-destructive.
  """
  fun unit[A](a: A): List[A] =>
    """
    Builds a new list from an element.
    """
    List[A].push(consume a)

  fun map[A: Any #read, B](l: List[A], f: {(A!): B^} val): List[B] =>
    """
    Builds a new list by applying a function to every member of the list.
    """
    try
      _map[A, B](l.head(), f, List[B])
    else
      List[B]
    end

  fun _map[A: Any #read, B](ln: ListNode[A], f: {(A!): B^} val, acc: List[B]):
  List[B] =>
    """
    Private helper for map, recursively working with ListNodes.
    """
    try acc.push(f(ln())) end

    try
      _map[A, B](ln.next() as ListNode[A], f, acc)
    else
      acc
    end

  fun flat_map[A: Any #read, B](l: List[A], f: {(A!): List[B]} val): List[B] =>
    """
    Builds a new list by applying a function to every member of the list and
    using the elements of the resulting lists.
    """
    try
      _flat_map[A,B](l.head(), f, List[List[B]])
    else
      List[B]
    end

  fun _flat_map[A: Any #read, B](ln: ListNode[A], f: {(A!): List[B]} val,
  acc: List[List[B]]): List[B] =>
    """
    Private helper for flat_map, recursively working with ListNodes.
    """
    try acc.push(f(ln())) end

    try
      _flat_map[A,B](ln.next() as ListNode[A], f, acc)
    else
      flatten[B](acc)
    end

  fun flatten[A](l: List[List[A]]): List[A] =>
    """
    Builds a new list out of the elements of the lists in this one.
    """
    let resList = List[A]
    for subList in l.values() do
      resList.append_list(subList)
    end
    resList

  fun filter[A: Any #read](l: List[A], f: {(A!): Bool} val): List[A] =>
    """
    Builds a new list with those elements that satisfy a provided predicate.
    """
    try
      _filter[A](l.head(), f, List[A])
    else
      List[A]
    end

  fun _filter[A: Any #read](ln: ListNode[A], f: {(A!): Bool} val,
  acc: List[A]): List[A] =>
    """
    Private helper for filter, recursively working with ListNodes.
    """
    try
      let cur = ln()
      if f(cur) then acc.push(consume cur) end
    end

    try
      _filter[A](ln.next() as ListNode[A], f, acc)
    else
      acc
    end

  fun fold[A: Any #read,B: Any #read](l: List[A], f: {(B!, A!): B^} val,
  acc: B): B =>
    """
    Folds the elements of the list using the supplied function.
    """
    try
      _fold[A,B](l.head(), f, acc)
    else
      acc
    end

  fun _fold[A: Any #read,B: Any #read](ln: ListNode[A], f: {(B!, A!): B^} val,
  acc: B!): B =>
    """
    Private helper for fold, recursively working with ListNodes.
    """
    let nextAcc: B! = try f(acc, ln()) else acc end

    try
      _fold[A,B](ln.next() as ListNode[A], f, nextAcc)
    else
      nextAcc
    end

  fun every[A: Any #read](l: List[A], f: {(A!): Bool} val): Bool =>
    """
    Returns true if every element satisfies the provided predicate, false
    otherwise.
    """
    try
      _every[A](l.head(), f)
    else
      true
    end

  fun _every[A: Any #read](ln: ListNode[A], f: {(A!): Bool} val): Bool =>
    """
    Private helper for every, recursively working with ListNodes.
    """
    try
      let a: A = ln()
      if not(f(a)) then
        false
      else
        _every[A](ln.next() as ListNode[A], f)
      end
    else
      true
    end

  fun exists[A: Any #read](l: List[A], f: {(A!): Bool} val): Bool =>
    """
    Returns true if at least one element satisfies the provided predicate,
    false otherwise.
    """
    try
      _exists[A](l.head(), f)
    else
      false
    end

  fun _exists[A: Any #read](ln: ListNode[A], f: {(A!): Bool} val): Bool =>
    """
    Private helper for exists, recursively working with ListNodes.
    """
    try
      let a: A = ln()
      if f(a) then
        true
      else
        _exists[A](ln.next() as ListNode[A], f)
      end
    else
      false
    end

  fun partition[A: Any #read](l: List[A], f: {(A!): Bool} val): (List[A],
  List[A]) =>
    """
    Builds a pair of lists, the first of which is made up of the elements
    satisfying the supplied predicate and the second of which is made up of
    those that do not.
    """
    let l1: List[A] = List[A]
    let l2: List[A] = List[A]
    for item in l.values() do
      if f(item) then l1.push(item) else l2.push(item) end
    end
    (l1, l2)

  fun drop[A: Any #read](l: List[A], n: USize): List[A] =>
    """
    Builds a list by dropping the first n elements.
    """
    if l.size() < (n + 1) then return List[A] end

    try
      _drop[A](l.clone().head(), n)
    else
      List[A]
    end

  fun _drop[A: Any #read](ln: ListNode[A], n: USize): List[A] =>
    """
    Private helper for drop, working with ListNodes.
    """
    var count = n
    var cur: ListNode[A] = ln
    while count > 0 do
      try cur = cur.next() as ListNode[A] else return List[A] end
      count = count - 1
    end
    let res = List[A]
    try res.push(cur()) end
    while cur.has_next() do
      try
        cur = cur.next() as ListNode[A]
        res.push(cur())
      end
    end
    res

  fun take[A: Any #read](l: List[A], n: USize): List[A] =>
    """
    Builds a list of the first n elements.
    """
    if l.size() <= n then l end

    try
      _take[A](l.clone().head(), n)
    else
      List[A]
    end

  fun _take[A: Any #read](ln: ListNode[A], n: USize): List[A] =>
    """
    Private helper for take, working with ListNodes.
    """
    var count = n
    let res = List[A]
    var cur: ListNode[A] = ln
    while count > 0 do
      try res.push(cur()) end
      try cur = cur.next() as ListNode[A] else return res end
      count = count - 1
    end
    res

  fun take_while[A: Any #read](l: List[A], f: {(A!): Bool} val): List[A] =>
    """
    Builds a list of elements satisfying the provided predicate until one does
    not.
    """
    try
      _take_while[A](l.clone().head(), f)
    else
      List[A]
    end

  fun _take_while[A: Any #read](ln: ListNode[A], f: {(A!): Bool} val): List[A]
  =>
    """
    Private helper for take_while, working with ListNodes.
    """
    let res = List[A]
    var cur: ListNode[A] = ln
    try
      let initial = cur()
      if f(initial) then res.push(initial) else return res end
    end
    while cur.has_next() do
      try cur = cur.next() as ListNode[A] else return res end
      try
        let value = cur()
        if f(value) then res.push(value) else return res end
      end
    end
    res

  fun contains[A: (Any #read & Equatable[A])](l: List[A], a: A): Bool =>
    """
    Returns true if the list contains the provided element, false otherwise.
    """
    try
      _contains[A](l.head(), a)
    else
      false
    end

  fun _contains[A: (Any #read & Equatable[A])](ln: ListNode[A], a: A): Bool =>
    """
    Private helper for contains, recursively working with ListNodes.
    """
    try
      if ln() == a then
        true
      else
        _contains[A](ln.next() as ListNode[A], a)
      end
    else
      false
    end

  fun reverse[A: Any #read](l: List[A]): List[A] =>
    """
    Builds a new list by reversing the elements in the list.
    """
    try
      _reverse[A](l.head(), List[A])
    else
      List[A]
    end

  fun _reverse[A: Any #read](ln: ListNode[A], acc: List[A]): List[A] =>
    """
    Private helper for reverse, recursively working with ListNodes.
    """
    try acc.unshift(ln()) end

    try
      _reverse[A](ln.next() as ListNode[A], acc)
    else
      acc
    end