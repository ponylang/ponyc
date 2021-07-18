## Fixed compiler build issues on FreeBSD host

Added relevant include and lib search paths under `/usr/local` 
prefix on FreeBSD, to satisfy build dependencies for Pony compiler.

## Make FileMode._os public

FileMode has always had a private method for getting an integer representation of the file mode. However, it was private and only available for use within the `files` package.

It is now public as `FileMode.os`.

