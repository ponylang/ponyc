trait Comparable[A: Comparable[A]]
  fun:box eq(that: A:box): Bool -> this is that
  fun:box ne(that: A:box): Bool -> not eq(that)

trait Ordered[A: Ordered[A]] is Comparable[A]
  fun:box lt(that: A:box): Bool
  fun:box le(that: A:box): Bool -> lt(that) or eq(that)
  fun:box ge(that: A:box): Bool -> not lt(that)
  fun:box gt(that: A:box): Bool -> not le(that)

trait Arithmetic[A: Arithmetic[A]]
  fun:box add(that: A:box): A:iso^
  fun:box sub(that: A:box): A:iso^
  fun:box mul(that: A:box): A:iso^
  fun:box div(that: A:box): A:iso^
  fun:box mod(that: A:box): A:iso^

trait Logical[A: Logical[A]]
  fun:box negate(): A:iso^
  fun:box disjunction(that: A:box): A:iso^
  fun:box conjunction(that: A:box): A:iso^
  fun:box exclusive(that: A:box): A:iso^

trait Shiftable[A: Shiftable[A]]
  fun:box lshift(): A:iso^
  fun:box rshift(): A:iso^
