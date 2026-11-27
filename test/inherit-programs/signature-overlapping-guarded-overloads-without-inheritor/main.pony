// T declares d three times: a plain body and two bodies guarded by iftype
// conditions that overlap. No type inherits T. The program must compile and
// print nothing.
trait T[A: Any val, B: Any val]
  fun d(a: A): String => "default"
  fun d(a: A): String iftype A <: Array[B] val => "arrB"
  fun d(a: A): String iftype A <: Array[U8] val => "arrU8"
actor Main
  new create(env: Env) => None
