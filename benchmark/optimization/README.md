# Optimization Benchmarks

Micro-benchmarks that verify Pony-specific optimizations survive the LTO pipeline. Each program runs a tight loop whose performance depends on a specific optimization firing. A regression in wall-clock time means the optimization is missing or incomplete.

Build and run a benchmark:

```
cd build/debug
./ponyc ../../benchmark/optimization/<name>
./<name>
```

Use a release build (`build/release`) for meaningful timings.

## Benchmarks

- **compute** — Raw numeric computation with no Pony object allocation. Covers LLVM's scalar optimization pipeline (strength reduction, loop optimization, constant folding). Baseline: a regression here means standard LTO optimization is weaker, not a Pony-specific pass problem.

- **cross-package** — HeapToStack promotion across package boundaries. Allocates objects defined in a separate package. The inliner must inline across the package boundary before HeapToStack can see the allocations as non-escaping.

- **heap2stack** — HeapToStack promotion of composite objects. Creates nested objects in a tight loop; both allocations must be promoted to avoid 400M heap allocations.

- **inline-heap2stack** — Inlining depth before HeapToStack. Allocates through a 3-level call chain that the inliner must collapse before HeapToStack can prove the allocation does not escape.

- **message-send** — MergeMessageSend survival through LTO. Sends 8 consecutive messages per iteration; MergeMessageSend chains them into a single batched send. A regression means the merged pattern did not survive lld's optimization pipeline.

- **vec-math** — Fluent vector math with temporary object chains. Arithmetic operations on Vec3 objects produce 4 intermediate allocations per iteration. HeapToStack must promote all temporaries.
