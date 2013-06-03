/*
FIX: don't want all functions with only iso|fro|opa args+res to have to work on
isolated receivers. is this actually a problem?

read: has to work on wri|fro, iso if args+res != wri.
write: has to work on wri, iso if args+res != wri.
frozen: only works on fro.
opaque: works on any.

iso
wri
fro
iso|wri
iso|fro
wri|fro
iso|wri|fro
opa

constructors with all iso|fro|opa arguments return an isolated object and others
return a writeable object. we can force the mode of the new object by using
<this> as the mode of a constructor argument.
*/


/*
example: have isolated LL, want to append frozen LL and get frozen LL
have to tear down isolated LL element by element, rebuilding as frozen
*/

class List[A] is Iterable[A]
{
  var item:A
  var next:List[A]<this>|None

  // works for any next, this is same mode as next
  new default(item:A, next:List[A]<this>|None = None) = this

  // works on iso|wri|fro
  read get() A->this = item

  // works on wri, iso if T<iso|fro|opa>
  write set(item_:A) A->this = swap(item, item_)

  // works on iso|wri|fro
  read get_next() List[A]<this>->this|None = next

  // works on iso|wri
  write set_next(next_:List[A]<this>|None) List[A]<this>|None = swap(next, next_)

  // works on wri|fro, not iso because of writeable result
  read iterator() ListIterator[A, this]<write> = ListIterator(this)
}

class ListIterator[A, p:wri|fro] is Itererator[A, p]
{
  var list:List[A]<p>

  new default(list:List[A]<p>) = this

  read has_next() Bool = match list.get_next()
    | as List[A]<p> = true
    | as None = false

  write next() A->p =
    var item = list.get();
    list = match list.get_next()
      | as a:List[A]<p> = a
      | as None = list
      ;
    item
}

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
