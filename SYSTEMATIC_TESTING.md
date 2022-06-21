# Systematic testing for the runtime

Pony is a concurrent and parallel language. Different actors can be run at the same time on multiple CPUs. The Pony runtime coordinates all of this interleaving of actors and contains a fair amount of complexity. Runtime functionality such as the message queues and the backpressure system rely on atomic operations which can be tricky to get right across multiple platforms.

Systematic testing allows for running of Pony programs in a deterministic manner. It accomplishes this by coordinating the interleaving of the multiple runtime scheduler threads in a deterministic and reproducible manner instead of allowing them all to run in parallel like happens normally. This ability to reproduce a particular runtime behavior is invaluable for debugging runtime issues.

The overall idea and some details of the implementation for systematic testing has been shamelessly stolen from the Verona runtime (see: https://github.com/microsoft/verona/blob/master/docs/explore.md#systematic-testing for details). This implementation doesn't include replayable runtime unit tests like Verona, but it sets a foundation for allowing replayable runs of programs (and probably tests) for debugging runtime issues such as backpressure/etc. Additionally, while all development and testing was done on Linux, in theory this systematic testing functionality should work on other operating systems (Windows, MacOS, Freebsd, etc) barring issues related to lack of atomics for tracking the active thread and whether a thread has stopped executing or not (unlikely to be an issue on MacOS/Freebsd/other `pthread` based threading implementations).

## Building

Instructions for how to build with systematic testing enabled can be found in [BUILD.md](BUILD.md).

The output of building and running `examples/helloworld` with systematic testing enabled will look something like:

```
me@home:~/ponyc$ ./helloworld
Systematic testing using seed: 360200870782547...
(rerun with `<app> --ponysystematictestingseed 360200870782547` to reproduce)
<SNIPPED LOTS OF OUTPUT>
thread 139871784978176: yielding to thread: 139871776585472.. next_index: 3
Hello, world.
thread 139871776585472: yielding to thread: 139871768192768.. next_index: 4
<SNIPPED LOTS OF OUTPUT>
Systematic testing successfully finished!
me@home:~/ponyc$ 
```

## Uses

As an example, if someone has a test that has an intermittent failure (that is somehow related to timing of how actors are scheduled and run) they could recompile the test with systematic testing enabled and then run the test until it fails and then continually reproduce the failure by re-using the same seed via the --ponysystematictestingseed <SEED_THAT_CAUSED_FAILURE> cli argument. Then once the intermittent failure can be reliably reproduced, it should make it significantly easier to track down the root cause and fix the bug.

NOTE: While systematic testing could be useful to users of ponyc (like in the example scenario), we expect it to get more use from developers of Pony as they enhance the runtime (i.e. changes to backpressure, changes to the message queue, changes to the objectmap, changes to GC, changes to the cycle detector, etc).