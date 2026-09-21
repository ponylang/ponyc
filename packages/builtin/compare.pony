primitive Less is Equatable[Compare]
  fun string(): String iso^ =>
    "Less".string()

primitive Equal is Equatable[Compare]
  fun string(): String iso^ =>
    "Equal".string()

primitive Greater is Equatable[Compare]
  fun string(): String iso^ =>
    "Greater".string()

type Compare is (Less | Equal | Greater)

interface HasEq[A]
  fun eq(that: box->A): Bool

interface Equatable[A: Equatable[A] #read]
  fun eq(that: box->A): Bool => this is that
  fun ne(that: box->A): Bool => not eq(that)

interface Comparable[A: Comparable[A] #read] is Equatable[A]
  """
  Total ordering. The default `compare`, `le`, `ge`, and `gt` assume `lt`
  defines a total order. Types with a partial order should implement
  `Equatable` instead and define their own comparison methods.
  """
  fun lt(that: box->A): Bool
  fun le(that: box->A): Bool => lt(that) or eq(that)
  fun ge(that: box->A): Bool => not lt(that)
  fun gt(that: box->A): Bool => not le(that)

  fun compare(that: box->A): Compare =>
    if eq(that) then
      Equal
    elseif lt(that) then
      Less
    else
      Greater
    end
