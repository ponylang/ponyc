use "lib:small-dependent-finalisers-additional"

use @codegentest_small_dependent_finalisers_decrement_num_objects[None]()

class _Final
  let num: USize
  var other: (_Final | None) = None
  
  new create(n: USize) =>
    num = n

  fun _final() =>
    match \exhaustive\ other
    | None => None
    | let o: this->_Final => 
      if 1025 == (num + o.num) then
        @codegentest_small_dependent_finalisers_decrement_num_objects()
      end
    end  

actor Main
  new create(env: Env) =>
    var i: USize = 0
    let arr: Array[_Final] = Array[_Final].create(1024)
    while i < arr.space() do
      i = i + 1
      arr.push(_Final(i))
    end
    i = 0
    while i < arr.space() do
      try
        arr(i)?.other = arr(arr.space() - (i + 1))?
      end
      i = i + 1
    end
