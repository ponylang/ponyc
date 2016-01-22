primitive Less is Equatable[Compare]
  fun string(fmt: FormatSettings = FormatSettingsDefault): String iso^
  =>
    "Less".string(fmt)

primitive Equal is Equatable[Compare]
  fun string(fmt: FormatSettings = FormatSettingsDefault): String iso^
  =>
    "Equal".string(fmt)

primitive Greater is Equatable[Compare]
  fun string(fmt: FormatSettings = FormatSettingsDefault): String iso^
  =>
    "Greater".string(fmt)


type Compare is (Less | Equal | Greater)

interface Equatable[A: Equatable[A] #read]
  fun eq(that: box->A): Bool => this is that
  fun ne(that: box->A): Bool => not eq(that)

interface Comparable[A: Comparable[A] #read] is Equatable[A]
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
