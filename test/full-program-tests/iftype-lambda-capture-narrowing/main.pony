use @pony_exitcode[None](code: I32)

class Wrapper[A: Any val]
  let _value: A
  new create(value: A) => _value = value

  fun test(): USize =>
    var count: USize = 0

    iftype A <: Stringable val then
      // Bare field capture
      let f1 = {()(_value): String => _value.string() }
      count = count + 1

      // Bare local capture
      let v = _value
      let f2 = {()(v): String => v.string() }
      count = count + 1

      // Expression capture (regression guard)
      let f3 = {()(w = _value): String => w.string() }
      count = count + 1

      // Object literal with field typed by A
      let obj = object
        let v: A = _value
        fun apply(): String => v.string()
      end
      count = count + 1

      // Tuple-typed capture exercises compound-type recursion
      let t: (A, A) = (_value, _value)
      let f4 = {()(t): String => t._1.string() }
      count = count + 1
    end

    count

class Pair[A: Any val, B: Any val]
  let _a: A
  let _b: B
  new create(a: A, b: B) => _a = a; _b = b

  fun test(): USize =>
    var count: USize = 0

    iftype A <: Stringable val then
      iftype B <: Stringable val then
        let f = {()(_a, _b): String => _a.string() + _b.string() }
        count = count + 1
      end
    end

    count

actor Main
  new create(env: Env) =>
    let w = Wrapper[U32](42)
    let p = Pair[U32, String](1, "hi")
    if (w.test() == 5) and (p.test() == 1) then
      @pony_exitcode(1)
    end
