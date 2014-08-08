trait Stringable
  fun box string(): String

class String val is Ordered[String]
  var size: U64
  var alloc: U64
  var ptr: _Pointer[U8]

  fun box length(): U64 => size

  fun box eq(that: String box): Bool => compiler_intrinsic
    /*if size == that.size then
      for i in Range[U64](0, size) do
        if ptr(i) != that.ptr(i) then
          return False
        end
      end*/


  fun box lt(that: String box): Bool => compiler_intrinsic
  /*fun box string(): String => this*/
