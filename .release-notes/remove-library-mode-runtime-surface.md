## Remove the rest of the library-mode runtime surface

Pony's library mode — embedding the runtime in a C host and stepping it by hand — was removed in earlier releases. This release removes the runtime C API functions that were left behind and that only library mode used.

These functions are gone: `pony_poll`, `pony_alloc_msg_size`, `pony_gc_acquire`, `pony_gc_release`, `pony_acquire_done`, `pony_release_done`, `pony_send`, `pony_sendp`, `pony_sendi`, `pony_register_thread`, and `pony_unregister_thread`.

Normal Pony programs are unaffected; the compiler never generated calls to any of these functions. Only C code that linked the runtime directly and called them is affected, and that code was using library mode, which is no longer supported.
