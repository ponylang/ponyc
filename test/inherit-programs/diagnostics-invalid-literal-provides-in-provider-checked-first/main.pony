// C provides T2, which holds T1's inherited f(), so T2 is checked before C.
// T2's g() has an object literal that provides U8. Expects the one "invalid
// provides type" error, with the compile stopping there as it does when T2 is
// checked on its own.
class C is T2
  fun g(): U8 => 0

trait T1
  fun f(): U8 => 1

trait T2 is T1
  fun g(): U8 =>
    let o = object is U8 end
    0

actor Main
  new create(env: Env) => None
