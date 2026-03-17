## Fix illegal instruction crashes in pony-lint, pony-lsp, and pony-doc

The tool build commands didn't pass `--cpu` to ponyc, so ponyc targeted the build machine's specific CPU via `LLVMGetHostCPUName()`. Release builds happen on CI machines that often have newer CPUs than user machines. When a user ran pony-lint, pony-lsp, or pony-doc on an older CPU, the binary could contain instructions their CPU doesn't support, resulting in a SIGILL crash.

The C/C++ side of the build already respected `PONY_ARCH` (e.g., `arch=x86-64` targets the baseline x86-64 instruction set). The Pony code generation for the tools just wasn't wired up to use it. Now it is.
