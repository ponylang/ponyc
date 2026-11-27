"""
A trait that gives a method's signature and body.
"""

trait Both
  """
  Declares and defines get.
  """
  fun get(): U64 =>
    let a: U64 = 40
    a + 2
