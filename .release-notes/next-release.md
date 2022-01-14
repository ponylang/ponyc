## Stop creating Centos 8 releases

CentOS 8 has been end-of-lifed and will be receiving no further updates. As per our policy, we will no longer be building regular releases for CentOS 8. Existing releases can still be installed via ponyup, but no additional releases will be done.

Our CentOS 8 support has been replaced by support for Rocky 8.

## Hide C header implementation details related to actor pad size.

Previously, in the `pony.h` header that exposes the "public API" of the Pony runtime, information about the size of an actor struct was published as public information.

This change removes that from the public header into a private header, to hide
implementation details that may be subject to change in future versions of the Pony runtime.

This change does not impact Pony programs - only C programs using the `pony.h` header to use the Pony runtime in some other way than inside of a Pony program, or to create custom actors written in C.

