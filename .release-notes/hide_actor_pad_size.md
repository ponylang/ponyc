## Hide C header implementation details related to actor pad size.

Previously, in the `pony.h` header that exposes the "public API" of the Pony runtime, information about the size of an actor struct was published as public information.

This change removes that from the public header into a private header, to hide
implementation details that may be subject to change in future versions of the Pony runtime.

This change does not impact Pony programs - only C programs using the `pony.h` header to use the Pony runtime in some other way than inside of a Pony program, or to create custom actors written in C.
