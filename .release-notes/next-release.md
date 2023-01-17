## Enable building of libponyc-standalone on MacOS

We now ship a "standalone" version of libponyc for MacOS. libponyc-standalone allows applications to use Pony compiler functionality as a library. The standalone version contains "all dependencies" needed in a single library. On MacOS, sadly "all dependencies" means "all that can be statically linked", so unlike Linux, dynamically linking to C++ standard library is required on MacOS.

An example pony program linking against it would need to look like this:

```pony
use "lib:ponyc-standalone" if posix or osx
use "lib:c++" if osx

actor Main
  new create(env: Env) =>
    None
```

## Update DTrace probes interface documentation

Our [DTrace]() probes needed some care. Over time, the documented interface and the interface itself had drifted. We've updated all probe interface definitions to match the current implementations.

## Improve usability of some DTrace probes

A number of runtime [DTrace]() probes were lacking information required to make them useful. The probes for:

- GC Collection Starting
- GC Collection Ending
- GC Object Receive Starting
- GC Object Receive Ending
- GC Object Send Starting
- GC Object Send Ending
- Heap Allocation

All lacked information about the actor in question. Instead they only contained information about the scheduler that was active at the time the operation took place. Without information about the actor doing a collection, allocating memory, etc the probes had little value.

