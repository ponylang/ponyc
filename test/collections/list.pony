class List[A] is Iterable[A]
  var item: A
  var next: (List[A] | None)

  new create(item': A, next': (List[A] | None)) =>
    item = consume item'
    next = next'

  // TODO: can't extend an immutable list
  // not a circularity problem
  // just need to have a next: List[A] imm to assign to while still this mut
  // but that means we have to know we will be imm
  // otherwise, could have a mut list with an imm next
  new immutable(item': A, next': (List[A] imm | None)) =>
    item = consume item'
    next = next' // TODO: can't do this

  fun box apply(): this.A => item

  fun mut update(a: A): this.A^ => item = a

  fun box get(): this.List[A] ? =>
    match next
    | as next': List[A] => next'
    | None => error
    end

  fun mut set(a: (List[A] | None)): (this.List[A] | None)^ => next = a

  fun mut push(a: A): List[A]^ => List(a, this)

  /*fun imm pushi(a: A): List[A] imm => ???*/

  fun box iterator(): ListIter[A, this.List[A]]^ => ListIter(this)

class ListIter[A, L: List[A] box] is Iterator[A]
  var list: (L | None)

  new create(list': (L | None)) => list = list'

  fun box has_next(): Bool =>
    match list
    | None => False
    | => True
    end

  fun mut next(): list.A ? =>
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
