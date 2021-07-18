## Fixed compiler build issues on FreeBSD host

Added relevant include and lib search paths under `/usr/local`
prefix on FreeBSD, to satisfy build dependencies for Pony compiler.

## Add FileMode.u32

Adds a public method on FileMode to get an OS-specific representation of the FileMode as a u32.
