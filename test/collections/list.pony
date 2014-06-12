class List[A] is Iterable[A]
  var item: A
  var next: (List[A] | None)

  new create(item': A, next': (List[A] | None)) =>
    item = consume item'
    next = next'

  // TODO: can't extend an immutable list
  // not a circularity problem
  // just need to have a next: List[A] val to assign to while still this ref
  // but that means we have to know we will be val
  // otherwise, could have a ref list with a val next
  new immutable(item': A, next': (List[A] val | None)) =>
    item = consume item'
    next = next' // TODO: can't do this

  fun box apply(): A this => item

  fun ref update(a: A): A this^ => item = a

  fun box get(): List[A] this ? =>
    match next
    | as next': List[A] => next'
    | None => error
    end

  fun ref set(a: (List[A] | None)): (List[A] this | None)^ => next = a

  fun ref push(a: A): List[A]^ => List(a, this)

  /*fun val pushi(a: A): List[A] val => ???*/

  fun box iterator(): ListIter[A, List[A] this]^ => ListIter(this)

class ListIter[A, L: List[A] box] is Iterator[A]
  var list: (L | None)

  new create(list': (L | None)) => list = list'

  fun box has_next(): Bool =>
    match list
    | None => False
    | => True
    end

  fun ref next(): A list ? =>
    var item = list()
    list = list.get()
    item

/*
vector
array
single list
double list
queue
stack
double-ended queue
tree
heap
bag
set
map
*/
