class Range[A:Number{val} = I64] is Iterator[A]
  val min:A
  val max:A
  val step:A
  var idx:A

  new create(min':A, max':A) =
    min = min';
    max = max';
    step = 0;
    idx = min

  fun has_next{var|val}():Bool = idx < max

  fun next{var}():A = if idx < max then idx = idx + step else idx
