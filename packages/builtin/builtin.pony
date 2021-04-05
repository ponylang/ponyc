"""
# Builtin package

The builtin package is home to the following standard library members:

1. Types the compiler needs to know exist, such as None.
2. Types with "magic" internal workings that must be supplied directly by the
compiler, such as U32.
3. Any types needed by others in builtin.

The public types that are defined in this package will always be in scope for
every Pony source file. For details on specific packages, see their individual
entity entries.
"""

use @pony_ctx[Pointer[None]]()
use @pony_alloc[Pointer[U8]](ctx: Pointer[None], size: USize)
use @pony_alloc_final[Pointer[U8]](ctx: Pointer[None], size: USize)
use @pony_exitcode[None](code: I32)
use @pony_get_exitcode[I32]()
use @pony_triggergc[None](ctx: Pointer[None])
use @ponyint_hash_block[USize](ptr: Pointer[None] tag, size: USize)
use @ponyint_hash_block64[U64](ptr: Pointer[None] tag, size: USize)
