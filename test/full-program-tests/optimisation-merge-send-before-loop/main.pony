"""
Regression test for #3784 / #5589.

A message send followed by a loop whose body sends to a different actor made
the MergeMessageSend optimisation pass re-run on its own output: the loop's
already-merged create block was inlined into the dispatch function, and the
guard meant to skip already-merged blocks could not see the pass's own
pony_chain calls, so the pass merged them a second time and crashed the
compiler (assertion in makeMsgChains). This must now compile and run.

All sends to Counter are ordered with respect to each other, so the exit code
is deterministic regardless of how Counter's and env.out's deliveries
interleave.

The crash depended on the optimiser inlining the already-merged create into the
dispatch function and re-running the pass over it. If a future LLVM stops doing
that, this would still compile and pass without exercising the guard's skip
path; after an LLVM upgrade, re-verify the trigger (the optimised IR for
Main_Dispatch should contain a pony_chain call).
"""
use @pony_exitcode[None](code: I32)

actor Counter
  var _count: I32 = 0

  be inc() =>
    _count = _count + 1

  be report() =>
    @pony_exitcode(_count)

actor Main
  new create(env: Env) =>
    let c = Counter
    c.inc()
    for s in ["a"; "b"; "c"].values() do
      env.out.print(s)
    end
    c.inc()
    c.inc()
    c.report()
