use "lib:large-dependent-finalisers-additional"

use @codegentest_large_dependent_finalisers_decrement_num_objects[None]()

class _Final
  let num: USize
  let num2: USize = 0
  let num3: USize = 0
  let num4: USize = 0
  let num5: USize = 0
  let num6: USize = 0
  let num7: USize = 0
  let num8: USize = 0
  let num9: USize = 0
  let num10: USize = 0
  let num11: USize = 0
  let num12: USize = 0
  let num13: USize = 0
  let num14: USize = 0
  let num15: USize = 0
  let num16: USize = 0
  let num17: USize = 0
  let num18: USize = 0
  let num19: USize = 0
  let num20: USize = 0
  let num21: USize = 0
  let num22: USize = 0
  let num23: USize = 0
  let num24: USize = 0
  let num25: USize = 0
  let num26: USize = 0
  let num27: USize = 0
  let num28: USize = 0
  let num29: USize = 0
  let num30: USize = 0
  let num31: USize = 0
  let num32: USize = 0
  let num33: USize = 0
  let num34: USize = 0
  let num35: USize = 0
  let num36: USize = 0
  let num37: USize = 0
  let num38: USize = 0
  let num39: USize = 0
  let num40: USize = 0
  let num41: USize = 0
  let num42: USize = 0
  let num43: USize = 0
  let num44: USize = 0
  let num45: USize = 0
  let num46: USize = 0
  let num47: USize = 0
  let num48: USize = 0
  let num49: USize = 0
  let num50: USize = 0
  let num51: USize = 0
  let num52: USize = 0
  let num53: USize = 0
  let num54: USize = 0
  let num55: USize = 0
  let num56: USize = 0
  let num57: USize = 0
  let num58: USize = 0
  let num59: USize = 0
  let num60: USize = 0
  let num61: USize = 0
  let num62: USize = 0
  let num63: USize = 0
  let num64: USize = 0
  let num65: USize = 0
  let num66: USize = 0
  let num67: USize = 0
  let num68: USize = 0
  let num69: USize = 0
  let num70: USize = 0
  var other: (_Final | None) = None
  
  new create(n: USize) =>
    num = n

  fun _final() =>
    match \exhaustive\ other
    | None => None
    | let o: this->_Final => 
      if 1025 == (num + o.num) then
        @codegentest_large_dependent_finalisers_decrement_num_objects()
      end
    end  

actor Main
  new create(env: Env) =>
    let f1 = _Final(1)
    let f2 = _Final(1024)
    f1.other = f2
    f2.other = f1
