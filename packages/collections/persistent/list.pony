type List[A] is (Cons[A] | Nil[A])
"""
A persistent list with functional transformations.

## Usage

```
let l1 = Lists[U32]([2, 4, 6, 8]) // List(2, 4, 6, 8)

let empty = Lists[U32].empty() // List()

// prepend() returns a new List, leaving the
// old list unchanged
let l2 = empty.prepend(3) // List(3)
let l3 = l2.prepend(2) // List(2, 3)
let l4 = l3.prepend(1) // List(1, 2, 3)
let l4_head = l4.head() // 1
let l4_tail = l4.tail() // List(2, 3)

h.assert_eq[U32](l4_head, 1)
h.assert_true(Lists[U32].eq(l4, Lists[U32]([1, 2, 3])))
h.assert_true(Lists[U32].eq(l4_tail, Lists[U32]([2, 3])))

let doubled = l4.map[U32](lambda(x: U32): U32 => x * 2 end)

h.assert_true(Lists[U32].eq(doubled, Lists[U32]([2, 4, 6])))

```
"""

primitive Lists[A]
  """
  A primitive containing helper functions for constructing and
  testing Lists.
  """

  fun empty(): List[A] =>
    """
    Returns an empty list.
    """
    Nil[A]

  fun cons(h: val->A, t: List[A]): List[A] =>
    """
    Returns a list that has h as a head and t as a tail.
    """
    Cons[A](h, t)

  fun apply(arr: Array[val->A]): List[A] =>
    """
    Builds a new list from an Array
    """
    var lst = this.empty()
    for v in arr.values() do
      lst = lst.prepend(v)
    end
    lst.reverse()

  fun from(iter: Iterator[val->A]): List[A] =>
    """
    Builds a new list from an iterator
    """
    var l: List[A] = Nil[A]

    for i in iter do
      l = Cons[A](i, l)
    end
    l

  fun eq[T: Equatable[T] val = A](l1: List[T], l2: List[T]): Bool ? =>
    """
    Checks whether two lists are equal.
    """
    if (l1.is_empty() and l2.is_empty()) then
      true
    elseif (l1.is_empty() and l2.is_non_empty()) then
      false
    elseif (l1.is_non_empty() and l2.is_empty()) then
      false
    elseif (l1.head() != l2.head()) then
      false
    else
      eq[T](l1.tail(), l2.tail())
    end

primitive Nil[A]
  """
  The empty list of As.
  """

  fun size(): U64 =>
    """
    Returns the size of the list.
    """
    0

  fun is_empty(): Bool =>
    """
    Returns a Bool indicating if the list is empty.
    """
    true

  fun is_non_empty(): Bool =>
    """
    Returns a Bool indicating if the list is non-empty.
    """
    false

  fun head(): val->A ? =>
    """
    Returns an error, since Nil has no head.
    """
    error

  fun tail(): List[A] ? =>
    """
    Returns an error, since Nil has no tail.
    """
    error

  fun reverse(): Nil[A] =>
    """
    The reverse of the empty list is the empty list.
    """
    this

  fun prepend(a: val->A!): Cons[A] =>
    """
    Builds a new list with an element added to the front of this list.
    """
    Cons[A](consume a, this)

  fun concat(l: List[A]): List[A] =>
    """
    The concatenation of any list l with the empty list is l.
    """
    l

  fun map[B](f: {(val->A): val->B} box): Nil[B] =>
    """
    Mapping a function from A to B over the empty list yields the
    empty list of Bs.
    """
    Nil[B]

  fun flat_map[B](f: {(val->A): List[B]} box): Nil[B] =>
    """
    Flatmapping a function from A to B over the empty list yields the
    empty list of Bs.
    """
    Nil[B]

  fun for_each(f: {(val->A)} box) =>
    """
    Applying a function to every member of the empty list is a no-op.
    """
    None

  fun filter(f: {(val->A): Bool} box): Nil[A] =>
    """
    Filtering the empty list yields the empty list.
    """
    this

  fun fold[B](f: {(B, val->A): B^} box, acc: B): B =>
    """
    Folding over the empty list yields the initial accumulator.
    """
    consume acc

  fun every(f: {(val->A): Bool} box): Bool =>
    """
    Any predicate is true of every member of the empty list.
    """
    true

  fun exists(f: {(val->A): Bool} box): Bool =>
    """
    For any predicate, there is no element that satisfies it in the empty
    list.
    """
    false

  fun partition(f: {(val->A): Bool} box): (Nil[A], Nil[A]) =>
    """
    The only partition of the empty list is two empty lists.
    """
    (this, this)

  fun drop(n: U64): Nil[A] =>
    """
    There are no elements to drop from the empty list.
    """
    this

  fun drop_while(f: {(val->A): Bool} box): Nil[A] =>
    """
    There are no elements to drop from the empty list.
    """
    this

  fun take(n: U64): Nil[A] =>
    """
    There are no elements to take from the empty list.
    """
    this

  fun take_while(f: {(val->A): Bool} box): Nil[A] =>
    """
    There are no elements to take from the empty list.
    """
    this

  fun val contains[T: (A & HasEq[A!] #read) = A](a: val->T): Bool =>
    false

class val Cons[A]
  """
  A list with a head and a tail, where the tail can be empty.
  """

  let _size: U64
  let _head: val->A
  let _tail: List[A] val

  new val create(a: val->A, t: List[A]) =>
    _head = consume a
    _tail = consume t
    _size = 1 + _tail.size()

  fun size(): U64 =>
    """
    Returns the size of the list.
    """
    _size

  fun is_empty(): Bool =>
    """
    Returns a Bool indicating if the list is empty.
    """
    false

  fun is_non_empty(): Bool =>
    """
    Returns a Bool indicating if the list is non-empty.
    """
    true

  fun head(): val->A =>
    """
    Returns the head of the list.
    """
    _head

  fun tail(): List[A] =>
    """
    Returns the tail of the list.
    """
    _tail

  fun val reverse(): List[A] =>
    """
    Builds a new list by reversing the elements in the list.
    """
    _reverse(this, Nil[A])

  fun val _reverse(l: List[A], acc: List[A]): List[A] =>
    """
    Private helper for reverse, recursively working on elements.
    """
    match l
    | let cons: Cons[A] => _reverse(cons.tail(), acc.prepend(cons.head()))
    else
      acc
    end

  fun val prepend(a: val->A!): Cons[A] =>
    """
    Builds a new list with an element added to the front of this list.
    """
    Cons[A](consume a, this)

  fun val concat(l: List[A]): List[A] =>
    """
    Builds a new list that is the concatenation of this list and the provided
    list.
    """
    _concat(l, this.reverse())

  fun val _concat(l: List[A], acc: List[A]): List[A] =>
    """
    Private helper for concat that recursively builds the new list.
    """
    match l
    | let cons: Cons[A] => _concat(cons.tail(), acc.prepend(cons.head()))
    else
      acc.reverse()
    end

  fun val map[B](f: {(val->A): val->B} box): List[B] =>
    """
    Builds a new list by applying a function to every member of the list.
    """
    _map[B](this, f, Nil[B])

  fun _map[B](l: List[A], f: {(val->A): val->B} box, acc: List[B]): List[B] =>
    """
    Private helper for map, recursively applying function to elements.
    """
    match l
    | let cons: Cons[A] =>
      _map[B](cons.tail(), f, acc.prepend(f(cons.head())))
    else
      acc.reverse()
    end

  fun val flat_map[B](f: {(val->A): List[B]} box): List[B] =>
    """
    Builds a new list by applying a function to every member of the list and
    using the elements of the resulting lists.
    """
    _flat_map[B](this, f, Nil[B])

  fun _flat_map[B](l: List[A], f: {(val->A): List[B]} box, acc: List[B]):
  List[B] =>
    """
    Private helper for flat_map, recursively working on elements.
    """
    match l
    | let cons: Cons[A] =>
      _flat_map[B](cons.tail(), f, _rev_prepend[B](f(cons.head()), acc))
    else
      acc.reverse()
    end

  fun tag _rev_prepend[B](l: List[B], target: List[B]): List[B] =>
    """
    Prepends l in reverse order onto target
    """
    match l
    | let cns: Cons[B] =>
      _rev_prepend[B](cns.tail(), target.prepend(cns.head()))
    else
      target
    end

  fun val for_each(f: {(val->A)} box) =>
    """
    Applies the supplied function to every element of the list in order.
    """
    _for_each(this, f)

  fun _for_each(l: List[A], f: {(val->A)} box) =>
    """
    Private helper for for_each, recursively working on elements.
    """
    match l
    | let cons: Cons[A] =>
      f(cons.head())
      _for_each(cons.tail(), f)
    end

  fun val filter(f: {(val->A): Bool} box): List[A] =>
    """
    Builds a new list with those elements that satisfy a provided predicate.
    """
    _filter(this, f, Nil[A])

  fun _filter(l: List[A], f: {(val->A): Bool} box, acc: List[A]): List[A] =>
    """
    Private helper for filter, recursively working on elements, keeping those
    that match the predicate and discarding those that don't.
    """
    match l
    | let cons: Cons[A] =>
      if (f(cons.head())) then
        _filter(cons.tail(), f, acc.prepend(cons.head()))
      else
        _filter(cons.tail(), f, acc)
      end
    else
      acc.reverse()
    end

  fun val fold[B](f: {(B, val->A): B^} box, acc: B): B =>
    """
    Folds the elements of the list using the supplied function.
    """
    _fold[B](this, f, consume acc)

  fun val _fold[B](l: List[A], f: {(B, val->A): B^} box, acc: B): B =>
    """
    Private helper for fold, recursively working on elements.
    """
    match l
    | let cons: Cons[A] =>
      _fold[B](cons.tail(), f, f(consume acc, cons.head()))
    else
      acc
    end

  fun val every(f: {(val->A): Bool} box): Bool =>
    """
    Returns true if every element satisfies the provided predicate, false
    otherwise.
    """
    _every(this, f)

  fun _every(l: List[A], f: {(val->A): Bool} box): Bool =>
    """
    Private helper for every, recursively testing predicate on elements,
    returning false immediately on an element that fails to satisfy the
    predicate.
    """
    match l
    | let cons: Cons[A] =>
      if (f(cons.head())) then
        _every(cons.tail(), f)
      else
        false
      end
    else
      true
    end

  fun val exists(f: {(val->A): Bool} box): Bool =>
    """
    Returns true if at least one element satisfies the provided predicate,
    false otherwise.
    """
    _exists(this, f)

  fun _exists(l: List[A], f: {(val->A): Bool} box): Bool =>
    """
    Private helper for exists, recursively testing predicate on elements,
    returning true immediately on an element satisfying the predicate.
    """
    match l
    | let cons: Cons[A] =>
      if (f(cons.head())) then
        true
      else
        _exists(cons.tail(), f)
      end
    else
      false
    end

  fun val partition(f: {(val->A): Bool} box): (List[A], List[A]) =>
    """
    Builds a pair of lists, the first of which is made up of the elements
    satisfying the supplied predicate and the second of which is made up of
    those that do not.
    """
    var hits: List[A] = Nil[A]
    var misses: List[A] = Nil[A]
    var cur: List[A] = this
    while(true) do
      match cur
      | let cons: Cons[A] =>
        let next = cons.head()
        if f(next) then
          hits = hits.prepend(next)
        else
          misses = misses.prepend(next)
        end
        cur = cons.tail()
      else
        break
      end
    end
    (hits.reverse(), misses.reverse())

  fun val drop(n: U64): List[A] =>
    """
    Builds a list by dropping the first n elements.
    """
    var cur: List[A] = this
    if cur.size() <= n then return Nil[A] end
    var count = n
    while(count > 0) do
      match cur
      | let cons: Cons[A] =>
        cur = cons.tail()
        count = count - 1
      end
    end
    cur

  fun val drop_while(f: {(val->A): Bool} box): List[A] =>
    """
    Builds a list by dropping elements from the front of the list until one
    fails to satisfy the provided predicate.
    """
    var cur: List[A] = this
    while(true) do
      match cur
      | let cons: Cons[A] =>
        if f(cons.head()) then cur = cons.tail() else break end
      else
        return Nil[A]
      end
    end
    cur

  fun val take(n: U64): List[A] =>
    """
    Builds a list of the first n elements.
    """
    var cur: List[A] = this
    if cur.size() <= n then return cur end
    var count = n
    var res: List[A] = Nil[A]
    while(count > 0) do
      match cur
      | let cons: Cons[A] =>
        res = res.prepend(cons.head())
        cur = cons.tail()
      else
        return res.reverse()
      end
      count = count - 1
    end
    res.reverse()

  fun val take_while(f: {(val->A): Bool} box): List[A] =>
    """
    Builds a list of elements satisfying the provided predicate until one does
    not.
    """
    var cur: List[A] = this
    var res: List[A] = Nil[A]
    while(true) do
      match cur
      | let cons: Cons[A] =>
        if f(cons.head()) then
          res = res.prepend(cons.head())
          cur = cons.tail()
        else
          break
        end
      else
        return res.reverse()
      end
    end
    res.reverse()
