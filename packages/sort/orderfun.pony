interface Sizeable
  fun size(): U64

interface Indexable[A]
  fun apply(i: U64): this->A ?
  fun ref update(i: U64, value: A): (A^ | None) ?

interface Sortable[A] is Sizeable, Indexable[A]

trait OrderFunction[A] val
  """
  A pluggable ordering function.
  """
  new val create()

  fun compare(x: this->A!, y: this->A!): I32

primitive OrderIdLt[A] is OrderFunction[A]
  """
  Orders elements by identity, lowest first.
  """
  fun compare(x: this->A!, y: this->A!): I32 =>
    if &x < &y then
      -1
    elseif &x == &y then
      0
    else
      1
    end

primitive OrderIdGt[A] is OrderFunction[A]
  """
  Orders elements by identity, highest first.
  """
  fun compare(x: this->A!, y: this->A!): I32 =>
    if &x > &y then
      -1
    elseif &x == &y then
      0
    else
      1
    end

primitive OrderLt[A: Ordered[A] box] is OrderFunction[A]
  """
  Orders elements by structural equality, least first.
  """
  fun compare(x: this->A!, y: this->A!): I32 =>
    if x < y then
      -1
    elseif x == y then
      0
    else
      1
    end

primitive OrderGt[A: Ordered[A] box] is OrderFunction[A]
  """
  Orders elements by structural equality, greatest first.
  """
  fun compare(x: this->A!, y: this->A!): I32 =>
    if x > y then
      -1
    elseif x == y then
      0
    else
      1
    end

primitive OrderSizeLt[A: Sizeable box] is OrderFunction[A]
  """
  Orders elements by size, smallest first.
  """
  fun compare(x: this->A!, y: this->A!): I32 =>
    if x.size() < y.size() then
      -1
    elseif x.size() == y.size() then
      0
    else
      1
    end

primitive OrderSizeGt[A: Sizeable box] is OrderFunction[A]
  """
  Orders elements by size, greatest first.
  """
  fun compare(x: this->A!, y: this->A!): I32 =>
    if x.size() > y.size() then
      -1
    elseif x.size() == y.size() then
      0
    else
      1
    end
