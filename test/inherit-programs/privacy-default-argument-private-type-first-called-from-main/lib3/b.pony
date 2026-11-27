primitive _H
  fun v(): U8 => 7
primitive Pub
  fun f(x: U8 = _H.v()): U8 => x
