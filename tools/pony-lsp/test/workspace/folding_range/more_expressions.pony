class MoreExpressions
  """
  Test fixture for additional expression-level folding ranges.
  One method per compound expression type not covered by expressions.pony.
  """
  fun with_for(): U32 =>
    var sum: U32 = 0
    for i in [as U32: 1; 2; 3].values() do
      sum = sum + i
    end
    sum

  fun with_repeat(): U32 =>
    var i: U32 = 0
    repeat
      i = i + 1
    until i >= 3 end
    i

  fun with_with(): None =>
    with d = _D do
      None
    end

  fun with_object(): String =>
    let obj =
      object
        fun value(): String => "hello"
      end
    obj.value()

  fun with_lambda(): String =>
    let f =
      {(): String =>
        "hello world"
      }
    f()

  fun with_recover(): String val =>
    recover
      let s = String.create()
      s.append("hello")
      consume s
    end

class _D
  new create() => None
  fun dispose(): None => None
