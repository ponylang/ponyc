"""
# Runtime Info package

The runtime information package exposes information about the Pony runtime that
can be queried at runtime. The most common usage at this time is limiting the
number of work based on the number of available schedulers.

For example, in an application that is doing parallel processing and wants to
limit the number of processing actors to the maximum number that could be run
at one time, you can use `Scheduler.schedulers` to get the scheduler
information.

```pony
use "collections"
use "runtime_info"

actor Processor

actor Main
  new create(env: Env) =>
    let s = Scheduler.schedulers(SchedulerInfoAuth(env.root))
    for i in Range(0, s) do
      Processor
    end
```
"""
