## Fix race conditions that can lead to a segfault

The [0.38.0](https://github.com/ponylang/ponyc/releases/tag/0.38.0) release introduced improvement to handling of short-lived actors. However, in the process it also introduced some race condition situations that could lead to segfaults. [0.38.1](https://github.com/ponylang/ponyc/releases/tag/0.38.1) introduced a fix that we hoped would address the problems, however, after that release we realized that it didn't fully address the situation. With this change, we've reworked the implementation of the short-lived actor improvements to avoid the trigger for the known race conditions entirely.

## Fix compiler crash when an if block ends with an assignment that has no result value

This release fixes a compiler crash that used to occur when certain kinds of assignment expressions that have no logical result value were used as the last expression in an `if` block, and possibly other control flow constructs as well. Now the compiler makes sure that the code generation for those cases always bears a value, even though the type system guarantees that the value will never be used in such a case. This prevents generating invalid LLVM IR blocks that have no proper terminator instruction.

