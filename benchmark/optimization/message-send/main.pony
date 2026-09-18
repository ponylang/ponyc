"""
Stress test for MergeMessageSend survival through LTO.

Sends 8 consecutive messages to the same actor per loop iteration.
MergeMessageSend should chain these into a single batched send (one
pony_sendv + 7 pony_chain calls). If the merged pattern doesn't survive
lld's O3 pipeline, each send becomes an independent pony_sendv with its own
allocation and trace round, measurably increasing overhead.

200M total messages from 25M iterations. The Sink actor processes them
sequentially (--ponymaxthreads=1), so timing covers both the send phase
(where merging matters) and the dispatch phase (constant baseline).
"""

actor Sink
  var _sum: U64 = 0
  let _main: Main tag

  new create(main': Main tag) =>
    _main = main'

  be take(x: U64) =>
    _sum = _sum + x

  be done() =>
    _main.finished(_sum)

actor Main
  let _env: Env

  new create(env: Env) =>
    _env = env
    let s = Sink(this)
    var i: U64 = 0
    while i < 200_000_000 do
      s.take(i)
      s.take(i + 1)
      s.take(i + 2)
      s.take(i + 3)
      s.take(i + 4)
      s.take(i + 5)
      s.take(i + 6)
      s.take(i + 7)
      i = i + 8
    end
    s.done()

  be finished(sum: U64) =>
    _env.out.print(sum.string())
