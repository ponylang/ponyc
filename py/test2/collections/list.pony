class List[A:Any{p}] is Iterable[A]
  var item:A
  var next:(List[A]{var}|None)

  fun default(item:A, next:(List[A]{var}|None) = None):List[A]{iso} =
    var a = \List[A];
    a.item = item;
    a.next = next;
    a.absorb()
//    \List[A](item->item, next->next).absorb()

  fun default(item:A{val}, next:(List[A]{val}|None) = None):List[A]{val} =
    \List[A](item->item, next->next).absorb()

  fun{iso|var|val} get_item():A{p->this} = item

  fun{iso|var} set_item(a:A):A = a = item

  fun{iso|var|val} get_next():(List[A]{var->this}|None) = next

  fun{iso|var} set_next(a:(List[A]{var}|None) = None) = a = next

  fun{var|val} iterator():ListIterator[A]{var} = ListIterator(this)

class ListIterator[A:Any{p}] is Iterator[A]
  var list:List[A]{var|val}

  fun default(list:List[A]{var|val}) =
    \ListIterator(list->list).absorb()

  fun{iso|var|val} has_next():Bool =
    match list.next()
    | None = false
    | = true
    end

  fun{iso|var} next():A{p->this} =
    var item = list.get();
    list = match list.get_next()
    | as a:List[A]{var|val} = a
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
