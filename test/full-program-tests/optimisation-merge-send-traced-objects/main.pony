"""
Regression test for #5589.

Consecutive message sends to the same actor that carry GC-traced arguments
exercise the MergeMessageSend trace-merge path: the first send keeps its
pony_gc_send trace round and later sends are rewritten to pony_send_next and
chained with pony_chain. Verify the merged sends still deliver every message
with the correct payload by summing the sizes of three strings (2 + 3 + 4).
"""
use @pony_exitcode[None](code: I32)

actor Sink
  var _total: I32 = 0

  be take(s: String) =>
    _total = _total + s.size().i32()

  be report() =>
    @pony_exitcode(_total)

actor Main
  new create(env: Env) =>
    let s = Sink
    s.take("aa")
    s.take("bbb")
    s.take("cccc")
    s.report()
