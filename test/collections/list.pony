class List[A] is Iterable[A]
  var item: A
  var next: (List[A]:var | None)

  // FIX: can't create an immutable list
  // not a circularity problem
  // just need to have a next:List[A]:val to assign to while still this:iso
  // but that means we have to know we will be val
  // otherwise, could have an iso or var list with a val tail
  new create(item': A, next': (List[A]:var | None)) ->
    item := item'
    next := next'

  fun:box apply(): this.A -> item

  fun:var update(a: A): this.A^ -> item := a

  fun:box? get(): this.List[A]:var ->
    match next
    | as a: List[A]:var -> a
    | None -> undef
    end

  fun:var set(a: (List[A]:var | None)): (this.List[A]:var^ | None) -> next := a

  fun:box iterator(): ListIter[A, this.List[A]:var]:var^ -> ListIter(this)

class ListIter[A, B:List[A]:box] is Iterator[A]
  var list:(B|None)

  new create(list':B) -> list := list'

  fun:box has_next(): Bool ->
    match list
    | None -> False
    | -> True
    end

  fun:var? next(): list.A =
    var item := list()
    list := list.get()
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
