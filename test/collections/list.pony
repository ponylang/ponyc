class List[A] is Iterable[A]
  var item:A
  var next:(List[A]{var}|None)

  // FIX: can't create an immutable list
  // not a circularity problem
  // just need to have a next:{val} to assign to while still this:{iso}
  // but that means we have to know we will be {val}
  // otherwise, could have an iso or var list with a val tail
  new{var} create(item':A, next':(List[A]{var}|None)) =
    item = item';
    next = next'

  // could parameterise the list with a type for the tail
  // but that's a much more complex type
  // esp since we only want a {val} tail for a {val} head
  // when constructing, we can promise to set all non-tag fields to {val}
  // but then we have to indicate we will always return a {val} object
  // cyclic graph of value types? handle with {free}
  // {free} is a subtype of {var}; is {iso} a subtype of {free}?
  // does {free} slot into the old freezable slot?
  new{val} value(item':A{val}, next':(List[A]{val}|None)) =
    // until we are initialised, we're a tag, so this is ok
    item = item';
    next = next'
    // now that we're initialised, we're a val, not a var

  fun{box} get_item():A->this = item

  fun{var} set_item(a:A):A = a = item

  fun{box} get_next():(List[A]{var}->this|None) = next

  fun{var} set_next(a:(List[A]{var}|None)) = a = next

  fun{box} iterator():ListIter[A, List[A]{this}]{var} = ListIter(this)

class ListIter[A, B:List[A]{box}] is Iterator[A]
  var list:B

  new{var} create(list':B) = list = list'

  fun{box} has_next():Bool =
    match list.get_next()
    | None = False
    | = True
    end

  fun{var} next():A->list =
    var item = list.get();
    list = match list.get_next()
    | as a:B = a
    | None = list
    end;
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
