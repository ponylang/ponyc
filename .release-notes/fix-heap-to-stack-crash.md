## Fix compiler crash in HeapToStack optimization pass on Windows

The compiler could crash during optimization when compiling programs that trigger the HeapToStack pass (which converts heap allocations to stack allocations). The crash was non-deterministic and most commonly observed on Windows, though it could theoretically occur on any platform.

The root cause was that HeapToStack was registered as a function-level optimization pass but performed operations (force-inlining constructors) that are only safe from a call-graph-level pass. This violated LLVM's pass pipeline contract and could cause use-after-free of internal compiler data structures.
