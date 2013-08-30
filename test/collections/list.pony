class List[A] is Iterable[A]
  var item:A
  var next:(List[A]|None)

  fun default(item':A{iso}, next':(List[A]{iso}|None) = None):List[A]{iso} =
    var a = Partial[This];
    a.item = item';
    a.next = next';
    a.absorb()

  fun mutable(item':A{var}, next':(List[A]{var}|None) = None):List[A]{var} =
    Partial[This](item'->item, next'->next).absorb()

  fun immutable(item':A{val}, next':(List[A]{val}|None) = None):List[A]{val} =
    Partial[This](item'->item, next'->next).absorb()

  fun get_item{iso|var|val}():A->this = item

  fun set_item{iso|var}(a:A):A = a = item

  fun get_next{iso|var|val}():(List[A]->this|None) = next

  fun set_next{iso|var}(a:(List[A]|None) = None) = a = next

  fun iterator{var|val}():ListIterator[A, List[A]{this}] = ListIterator(this)

class ListIterator[A, B:List[A]]{var} is Iterator[A]
  var list:B

  fun default(list':B) =
    Partial[This](list'->list).absorb()

  fun has_next():Bool =
    match list.get_next()
    | None = False
    | = True
    end

  fun next():A->list =
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
