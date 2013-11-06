class List[A] is Iterable[A]
  var item:A
  var next:(List[A]|None)

  new create(item':A, next':(List[A]|None) = None) =
    item = item';
    next = next'

  fun get_item{iso|var|val}():A->this = item

  fun set_item{iso|var}(a:A):A = a = item

  fun get_next{iso|var|val}():(List[A]->this|None) = next

  fun set_next{iso|var}(a:(List[A]|None) = None) = a = next

  fun iterator{var|val}():ListIter[A, List[A]] = ListIter(this)

class ListIter[A, B:List[A]] is Iterator[A]
  var list:B

  new create(list':B) = list = list'

  fun has_next{iso|var|val}():Bool =
    match list.get_next()
    | None = False
    | = True
    end

  fun next{var}():A->list =
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
