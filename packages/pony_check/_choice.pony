class val _IntChoice is (Equatable[_IntChoice] & Stringable)
  let value: I128
  let min: I128
  let max: I128
  let shrink_towards: I128

  new val create(
    value': I128,
    min': I128,
    max': I128,
    shrink_towards': I128 = 0)
  =>
    value = value'
    min = min'
    max = max'
    shrink_towards = shrink_towards'

  fun eq(other: box->_IntChoice): Bool =>
    (value == other.value) and (min == other.min) and
      (max == other.max) and (shrink_towards == other.shrink_towards)

  fun string(): String iso^ =>
    recover
      String()
        .> append("IntChoice(")
        .> append(value.string())
        .> append(", min=")
        .> append(min.string())
        .> append(", max=")
        .> append(max.string())
        .> append(", towards=")
        .> append(shrink_towards.string())
        .> append(")")
    end

class val _FloatChoice is (Equatable[_FloatChoice] & Stringable)
  let value: F64
  let min: F64
  let max: F64

  new val create(value': F64, min': F64, max': F64) =>
    value = value'
    min = min'
    max = max'

  fun eq(other: box->_FloatChoice): Bool =>
    (value == other.value) and (min == other.min) and (max == other.max)

  fun string(): String iso^ =>
    recover
      String()
        .> append("FloatChoice(")
        .> append(value.string())
        .> append(", min=")
        .> append(min.string())
        .> append(", max=")
        .> append(max.string())
        .> append(")")
    end

class val _BoolChoice is (Equatable[_BoolChoice] & Stringable)
  let value: Bool

  new val create(value': Bool) =>
    value = value'

  fun eq(other: box->_BoolChoice): Bool =>
    value == other.value

  fun string(): String iso^ =>
    ("BoolChoice(" + value.string() + ")").string()

type _Choice is (_IntChoice | _FloatChoice | _BoolChoice)
