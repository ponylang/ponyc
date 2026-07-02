// Regression test for https://github.com/ponylang/ponyc/issues/4507
//
// Ensures that the boxing fix does not double-box values that are already
// object pointers.  A prior fix attempt (PR #4787) caused Container("1234")
// to appear as Container(Container(1234)) due to double-boxing.

class val Container
  var data: Any val
  new val create(data': Any val) =>
    data = data'

primitive GetStringable
  fun val apply(p: Container): String =>
    "Container(" +
      match p.data
      | let s: Stringable => s.string()
      | let p': Container => GetStringable(p')
      else
        "Non-Stringable"
      end +
    ")"

actor Main
  new create(env: Env) =>
    let arg = Container((
        "abcd",
        Container("1234")
      ))

    match arg.data
    | (let s: String, let p: (Container | None)) =>
      if s != "abcd" then
        env.exitcode(1)
        return
      end

      let inner =
        match p
        | let p_cv: Container => GetStringable(consume p_cv)
        else
          "non-Container"
        end

      if inner != "Container(1234)" then
        env.exitcode(1)
      end
    else
      env.exitcode(1)
    end
