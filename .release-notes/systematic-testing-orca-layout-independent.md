## Make systematic testing replay independent of memory layout for reference-counting messages

Under `use=systematic_testing`, replaying a run from a fixed `--ponysystematictestingseed` is meant to reproduce the same scheduler interleaving. For workloads that pass references between actors — anything that drives the runtime's reference-counting (acquire/release) messages — a fixed seed could still replay a different interleaving from one run to the next, because those messages were sent in an order that depended on actor memory addresses, which the operating system randomizes per run. Replaying such a workload now reproduces the same interleaving regardless of address-space layout.

This builds on the earlier fix for the actor muting path. With both in place and the cycle detector disabled (`--ponynoblock`), a fixed seed replays deterministically for these workloads; the cycle detector has its own remaining layout dependence, tracked separately.
