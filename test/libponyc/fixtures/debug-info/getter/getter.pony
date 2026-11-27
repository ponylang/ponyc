"""
An object literal written in this package.
"""

interface Getter
  """
  Has get.
  """
  fun get(): U64
    """
    Returns a number.
    """

primitive Make
  """
  Makes a Getter from an object literal.
  """
  fun apply(): Getter val =>
    object
      fun get(): U64 =>
        let a: U64 = 40
        a + 2
    end
