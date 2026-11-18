interface \nodoc\ _Closure
  fun apply(x: U64): U64

trait \nodoc\ _Signature
  fun \nomutate\ get(): _Closure val

trait \nodoc\ _Donor[A: Any val]
  fun get(): _Closure val =>
    {(x: U64): U64 =>
      let increment: U64 = iftype A <: U64 then 1 else 2 end
      x + increment
    }
