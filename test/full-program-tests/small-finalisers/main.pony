use "lib:small-finalisers-additional"

use @codegentest_small_finalisers_increment_num_objects[None]()

class _Final
  fun _final() =>
    @codegentest_small_finalisers_increment_num_objects()

actor Main
  new create(env: Env) =>
    var i: U64 = 0
    while i < 42 do
      _Final
      i = i + 1
    end
