actor Main
  new create(env: Env) =>
    let p: HtmlNode = P
    p.apply()

class P is HtmlNode
  fun apply(): None  =>
    for f in [as HtmlNode:].values() do
      None
    end

interface HtmlNode
  fun apply(): None
