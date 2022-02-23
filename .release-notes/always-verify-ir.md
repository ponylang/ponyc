## Turn on `verify` pass by default

The `verify` compiler pass will check the LLVM IR generated for the program being compiled for errors. Previously, you had to turn verify on. It is now on by default and can be turned off using the new `--noverify` option.

We decided to turn on the pass for two reasons. The first is that it adds very little overhead to normal compilation times. The second is it will help generate error reports from Pony users for lurking bugs in our code generation.

The errors reported don't generally result in incorrect programs, but could under the right circumstance. We feel that turning on the reports will allow us to find and fix bugs quicker and help move us closer to Pony version 1.
