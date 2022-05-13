## Update glibc Linux Docker base image

Previously our glibc Linux Docker images where based on Ubuntu 20.04. They've been updated to use Ubuntu 22.04.

## Update to LLVM 14.0.3

We've updated the LLVM used to build Pony to 14.0.3.

## Don't include debug information in release versions of ponyc

At some point in the past, we turned on an option PONY_ALWAYS_ASSERT when building ponyc and the runtime. The result of this option was to not only turn on all debug assertions, but also, turn on almost all debug code in the runtime and the ponyc compiler.

The inclusion of assertions was great for error reports from users for compiler bugs. However, it was also including code that would make the compiler slower and use more memory. It was also including code that made the runtime slower for all compiled Pony programs.

We've turned off PONY_ALWAYS_ASSERT. Programs will be somewhat faster, the compiler will be a little faster, and the compiler will use a little less memory. In return, if you report a compiler bug, we'll definitely need a minimal reproduction to have any idea what is causing your bug.

