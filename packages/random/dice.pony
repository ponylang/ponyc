class Dice
  """
  A simple dice roller.
  """
  var r: Random

  new create(from: Random) =>
    """
    Initialise with a random number generator.
    """
    r = from

  fun ref apply(count: U64, sides: U64): U64 =>
    """
    Return the sum of `count` rolls of a die with the given number of `sides`.
    The die is numbered from 1 to `sides`. For example, count = 2 and
    sides = 6 will return a value between 2 and 12.
    """
    var sum = count
    var i: U64 = 0

    while i < count do
      sum = sum + r.int(sides)
      i = i + 1
    end
    sum
