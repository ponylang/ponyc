"""
A trait that gives the body of a method another trait declares.
"""
use s = "sig"

trait Donor is s.Sig
  """
  Gives get a body.
  """
  fun get(): U64 =>
    let a: U64 = 40
    a + 2
