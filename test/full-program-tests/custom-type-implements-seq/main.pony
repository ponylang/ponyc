use "collections"

class FixedPair is Seq[String]
  var _first: String
  var _second: String

  new create(a: String, b: String) =>
    _first = a
    _second = b

  fun size(): USize => 2

  fun apply(i: USize): this->String ? =>
    match i
    | 0 => _first
    | 1 => _second
    else error
    end

  fun ref update(i: USize, value: String): String^ ? =>
    match i
    | 0 => _first = consume value
    | 1 => _second = consume value
    else error
    end

  fun values(): Iterator[this->String]^ =>
    let arr = Array[this->String](2)
    arr.push(_first)
    arr.push(_second)
    arr.values()

actor Main
  new create(env: Env) =>
    let p = FixedPair("beta", "alpha")
    Sort[FixedPair, String](p)
    try
      env.out.print(p(0)?)
      env.out.print(p(1)?)
    end
