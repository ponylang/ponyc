class List[A] is Iterable[A]
  var item:A
  var next:(List[A]|None)

  fun default(item:A{iso}, next:(List[A]{iso}|None) = None):List[A]{iso} =
    var a = \List[A];
    a.item = item;
    a.next = next;
    a.absorb()
//    \List[A](item->item, next->next).absorb()

  fun default(item:A{var}, next:(List[A]{var}|None) = None):List[A]{var} =
    \List[A](item->item, next->next).absorb()

  fun default(item:A{val}, next:(List[A]{val}|None) = None):List[A]{val} =
    \List[A](item->item, next->next).absorb()

  fun{iso|var|val} get_item():A->this = item

  fun{iso|var} set_item(a:A):A = a = item

  fun{iso|var|val} get_next():(List[A]->this|None) = next

  fun{iso|var} set_next(a:(List[A]|None) = None) = a = next

  fun{var|val} iterator():ListIterator[A, List[A]{this}] = ListIterator(this)

class ListIterator[A, B:List[A]]{var} is Iterator[A]
  var list:B

  fun default(list:B) =
    \ListIterator(list->list).absorb()

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
