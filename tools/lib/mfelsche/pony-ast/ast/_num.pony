use "pony_test"
use "debug"

primitive _Num
  """
  Utility for checking the length of numbers.
  """
  fun int(src: String box, start_offset: USize, only_decimal: Bool = false): USize =>
    """
    returns the offset of the last character of the integer in src at start_offset
    """
    var offset = start_offset
    var c = try src(offset)? else return offset end
    let predicate =
      if only_decimal then
        _ASCII~is_decimal()
      else
        match c
        | '0' =>
          let c1 = try src(offset + 1)? else return offset end
          if c1 == 'x' then
            offset = offset + 2
            _ASCII~is_hexadecimal()
          elseif c1 == 'b' then
            offset = offset + 2
            _ASCII~is_binary()
          else
            _ASCII~is_decimal()
          end
        | '_' =>
          // special case for tuple element references
          // which are rewritten to TK_INT
          // as a hack we increase the offset past the underscore to get a
          // valid offset
          offset = offset + 1
          _ASCII~is_decimal()
        else
          _ASCII~is_decimal()
        end
      end
    c = try src(offset)? else return offset - 1 end
    while predicate(c) do
      offset = offset + 1
      c = try src(offset)? else return offset - 1 end
    end
    offset - 1
  
  fun float(src: String box, start_offset: USize): USize =>
    """returns the offset of the last character of the float in src at start_offset"""
    // integer base 10
    var offset = int(src, start_offset where only_decimal = true)
    Debug("integral part end: " + offset.string())

    offset = offset + 1
    
    // optional point + significand
    var c = try src(offset)? else return offset - 1 end
    if c == U8('.') then
      offset = offset + 1
      offset = int(src, offset where only_decimal= true)
      Debug("significand end: " + offset.string())
    end
    offset = offset + 1
    // optional exponent
    // (e | E) (+| -) integer base 10
    c = try src(offset)? else return offset - 1 end
    if (c == U8('e')) or (c == U8('E')) then
      offset = offset + 1
      c = try src(offset)? else return offset - 1 end
      if (c == U8('-')) or (c == U8('+')) then
        offset = offset + 1
      end
      let exponent_end_offset = int(src, offset where only_decimal = true)
      Debug("exponent end: " + exponent_end_offset.string())
      return exponent_end_offset
    else
      offset - 1
    end

class _NumTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_FloatTest)
    test(_IntTest)

class iso _FloatTest is UnitTest
  fun name(): String => "num/float"
  fun apply(h: TestHelper) =>
    var res = _Num.float("12345", 0)
    h.assert_eq[USize](4, res)
    
    res = _Num.float("12345.67890e+12345", 0)
    h.assert_eq[USize](17, res)
    
class iso _IntTest is UnitTest
  fun name(): String => "num/int"
  fun apply(h: TestHelper) =>
    let res = _Num.int("0", 0)
    h.assert_eq[USize](0, res)
