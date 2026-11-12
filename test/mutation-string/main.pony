type _Text is String

class \nodoc\ _Results
  var calls: U64 = 0

  fun ref val_result(): String val =>
    calls = calls + 1
    "original"

  fun ref box_result(): String box =>
    calls = calls + 1
    "original"

  fun ref ref_result(): String ref =>
    calls = calls + 1
    String .> append("original")

  fun ref iso_result(): String iso^ =>
    calls = calls + 1
    recover iso String .> append("original") end

  fun ref trn_result(): String trn^ =>
    calls = calls + 1
    recover trn String .> append("original") end

  fun ref tag_result(value: String val): String tag =>
    calls = calls + 1
    value

  fun ref alias_result(): _Text =>
    calls = calls + 1
    "original"

  fun ref explicit_result(early: Bool): String =>
    calls = calls + 1
    if early then return "original" end
    "fallback"

class \nodoc\ _Generic[A: Any val]
  fun apply(value: A): A => value

actor \nodoc\ Main
  new create(env: Env) =>
    let r: _Results ref = _Results
    env.out.print("val=" + r.val_result().size().string())
    env.out.print("box=" + r.box_result().size().string())
    let a = r.ref_result()
    let ref_size = a.size()
    let ref_ptr = a.cpointer()
    if not a.is_null_terminated() then env.exitcode(1) end
    a.truncate(0)
    a.append("x")
    let b = r.ref_result()
    env.out.print("ref=" + ref_size.string() + ":" + b.size().string())
    if (a.size() != 1) or (a != "x") or
      (a.cpointer() != ref_ptr) or (a is b)
    then
      env.exitcode(1)
    end
    let c = r.iso_result()
    let iso_size = c.size()
    if not c.is_null_terminated() then env.exitcode(1) end
    c.truncate(0)
    c.append("x")
    env.out.print("iso=" + iso_size.string())
    if (c.size() != 1) or (c != "x") then env.exitcode(1) end
    let d = r.trn_result()
    let trn_size = d.size()
    if not d.is_null_terminated() then env.exitcode(1) end
    d.truncate(0)
    d.append("x")
    env.out.print("trn=" + trn_size.string())
    if (d.size() != 1) or (d != "x") then env.exitcode(1) end
    let original = "original"
    env.out.print("tag=" + (r.tag_result(original) is original).string())
    env.out.print("alias=" + r.alias_result().size().string())
    env.out.print("explicit=" + r.explicit_result(true).size().string())
    env.out.print("generic=" +
      _Generic[String].apply("original").size().string())
    env.out.print("calls=" + r.calls.string())
