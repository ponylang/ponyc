## Fix "iftype" expressions not being usable in lambdas or object literals

This release fixes an issue where the compiler would hit an assertion error when compiling programs that placed `iftype` expressions inside lambdas or object literals.

## Fix code generation for variadic FFI functions on arm64

This release fixes an issue with code generation of variadic functions on arm64 architectures. The [arm64 calling convention](https://github.com/ARM-software/abi-aa/blob/23c65b71c40a5e3482eb89b46fc2ac9423947f20/aapcs64/aapcs64.rst) specifies that the named arguments of a variadic function (those arguments that are not optional) must be placed on registers, or on the stack if there are no available registers. Anonymous arguments (those that are optional) must always be placed on the stack. The Pony compiler would treat all variadic functions as if all their arguments were anonymous, thus placing every argument on the stack, even if there were available registers. This would cause issues on the C side of a variadic function, as programs would try and read from registers first, potentially finding uninitialized values.

