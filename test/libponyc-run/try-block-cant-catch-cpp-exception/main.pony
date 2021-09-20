"""
This test doesn't work on Windows. The C++ code doesn't catch C++ exceptions
if they've traversed a Pony frame. This is suspected to be related to how
SEH and LLVM exception code generation interact.

See https://github.com/ponylang/ponyc/issues/2455 for more details.
"""
use "path:./"
use "lib:try"

use @codegen_test_tryblock_catch[I32](callback: Pointer[None])
use @codegen_test_tryblock_throw[None]()?
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let r = @codegen_test_tryblock_catch(this~tryblock())
    @pony_exitcode(r)

  fun @tryblock() =>
    try
      @codegen_test_tryblock_throw()?
    else
      None
    end
