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
