## Fix compiler crash when an if block ends with an assignment that has no result value

This release fixes a compiler crash that used to occur when certain kinds of assignment expressions that have no logical result value were used as the last expression in an `if` block, and possibly other control flow constructs as well. Now the compiler makes sure that the code generation for those cases always bears a value, even though the type system guarantees that the value will never be used in such a case. This prevents generating invalid LLVM IR blocks that have no proper terminator instruction.
