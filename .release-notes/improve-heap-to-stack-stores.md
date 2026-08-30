## Improve heap-to-stack promotion for stored objects

The heap-to-stack optimization pass now promotes objects that are stored into fields of already-stack-promoted parents. Previously, storing a `pony_alloc`'d pointer into any field caused the pass to treat the allocation as escaped, keeping it on the heap even when the parent was local and never left the function.

A common case this unlocks: a local `String` whose backing buffer is a separate allocation stored into a String field. When the String is promoted to the stack, the buffer can now follow it.
