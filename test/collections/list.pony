class List[A] is Iterable[A]
  var item: A
  var next: (List[A] mut | None)

  // FIX: can't create an immutable list
  // not a circularity problem
  // just need to have a next:List[A] imm to assign to while still this iso
  // but that means we have to know we will be val
  // otherwise, could have an iso or var list with a val tail
  new create(item': A, next': (List[A] mut | None)) =>
    item = item'
    next = next'

  fun box apply(): this.A => item

  fun mut update(a: A): this.A^ => item = a

  fun mut get(): this.List[A] mut ? =>
    match next
    | as a: List[A] mut => a
    | None => error
    end

  fun mut set(a: (List[A] mut | None)): (this.List[A] mut | None)^ => next = a

  fun mut iterator(): ListIter[A, this.List[A] mut] mut^ => ListIter(this)

class ListIter[A, B: List[A] box] is Iterator[A]
  var list: (B|None)

  new create(list': B) => list = list'

  fun mut has_next(): Bool =>
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
