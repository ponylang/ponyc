use builtin_types = "builtin"
use @pony_exitcode[None](code: I32)

trait Signature
  fun check(): U64

trait GuardedSignature[A: Any val]
  fun check(): U64 iftype A <: U8

primitive \nodoc\ SignatureFirst[B: Any val] is (Signature & Group[B])

primitive \nodoc\ GuardedSignatureFirst[A: Any val] is
  (GuardedSignature[A] & Group[A])

primitive \nodoc\ GroupFirst[B: Any val] is (Group[B] & Signature)

primitive \nodoc\ LocalDefault[B: Any val] is Group[B]
  fun check(): U64 => 30

primitive \nodoc\ LocalSpecialization[B: Any val] is Group[B]
  fun check(): U64 iftype B <: (U8 | builtin_types.U16) => 40

primitive \nodoc\ LocalExtraSpecialization[B: Any val] is Group[B]
  fun check(): U64 iftype B <: U64 => 50

actor \nodoc\ Main
  new create(env: Env) =>
    if
      (SignatureFirst[U32].check() == 10) and
      (SignatureFirst[U8].check() == 20) and
      (SignatureFirst[U16].check() == 21) and
      (GuardedSignatureFirst[U8].check() == 20) and
      (GroupFirst[U32].check() == 10) and
      (GroupFirst[U8].check() == 20) and
      (GroupFirst[U16].check() == 21) and
      (LocalDefault[U32].check() == 30) and
      (LocalDefault[U8].check() == 20) and
      (LocalDefault[U16].check() == 21) and
      (LocalSpecialization[U32].check() == 10) and
      (LocalSpecialization[U8].check() == 20) and
      (LocalSpecialization[U16].check() == 40) and
      (LocalExtraSpecialization[U32].check() == 10) and
      (LocalExtraSpecialization[U8].check() == 20) and
      (LocalExtraSpecialization[U16].check() == 21) and
      (LocalExtraSpecialization[U64].check() == 50)
    then
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
