class Dice
  var r: Random

  new create(from: Random) => r = from

  fun ref apply(count: U64, sides: U64): U64 =>
    var sum = count
    var i: U64 = 0

    while i < count do
      sum = sum + r.int(sides)
      i = i + 1
    end
    sum
