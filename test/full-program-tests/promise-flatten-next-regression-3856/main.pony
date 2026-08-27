use "promises"

actor FlattenNextProbe
  new create(handler: Handler iso) =>
    let promise = Promise[String]
    promise.next[Any tag](recover this~behaviour() end)
    (consume handler)(recover String end, promise)

  be behaviour(value: String) =>
    None


class Handler
  fun ref apply(line: String, prompt: Promise[String]) =>
    let p = Promise[String]
    p.flatten_next[String]({ (x: String) => Promise[String] })


actor Main
  new create(env: Env) =>
    let term = FlattenNextProbe(Handler)