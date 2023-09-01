use "promises"

actor FlattenNextProbe
  new create(handler: Handler iso) =>
    let promise = Promise[String]
    // We need a call to next to be present with a call to flatten_next to trigger the issue during reachability analysis
    promise.next[Any tag](recover this~behaviour() end)
    (consume handler)(recover String end, promise)

  be behaviour(value: String) =>
    None


class Handler
  fun ref apply(line: String, prompt: Promise[String]) =>
    let p = Promise[String]
    // BUG: Can't change the flatten_next
    p.flatten_next[String]({ (x: String) => Promise[String] })


actor Main
  new create(env: Env) =>
    // BUG: Can't change this
    let term = FlattenNextProbe(Handler)