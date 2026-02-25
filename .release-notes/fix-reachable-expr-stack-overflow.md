## Fix stack overflow in reachability pass on deeply nested ASTs

The reachability pass traversed AST trees using unbounded recursion, adding a stack frame for each child node. On Windows x86-64, which has a default stack size of 1MB, this could overflow when compiling programs with large type graphs that produce deeply nested ASTs (200+ frames deep).

The recursive child traversal is now iterative, using an explicit worklist. This removes the dependency on call stack depth for AST traversal in the reachability pass.
