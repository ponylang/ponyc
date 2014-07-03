class ListNode[A]
  var _item: A
  var _next: (ListNode[A] | None)

  new create(item': A, next': (ListNode[A] | None)) =>
    _item = item'
    _next = next'

  fun box item(): this->A => _item

  fun box next(): this->(ListNode[A] | None) => _next

class List[A] is Iterable[A]
  var _head: (ListNode[A] | None)

  new create() => _head = None

  fun ref push(a: A) => _head = ListNode[A](a, _head)

  fun box iterator(): ListIterator[A, this->ListNode[A]]^ =>
    ListIterator[A, this->ListNode[A]](_head)

class ListIterator[A, N: ListNode[A] box] is Iterator[N->A]
  var _next: (N | None)

  new create(head: (N | None)) => _next = head

  fun box has_next(): Bool => _next isnt None

  fun ref next(): N->A ? =>
    match _next
    | as next': N =>
      _next = next'.next()
      next'.item()
    else
      error
    end
