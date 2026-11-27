trait \nodoc\ _Signature
  fun \nomutate\ get(): U64

trait \nodoc\ _Donor
  fun get(): U64 => U64(1) + 2
  fun mk(): _Donor val => object is (_Signature & _Donor) end

class \nodoc\ _Holder is _Donor
  new create() => None
