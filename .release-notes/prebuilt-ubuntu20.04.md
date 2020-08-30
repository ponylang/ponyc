## Add prebuilt ponyc binaries for Ubuntu 20.04

We've added prebuilt ponyc binaries specifically made to work on Ubuntu 20.04. At the time of this change, our generic glibc binaries are built on Ubuntu 20.04 and guaranteed to work on it. However, at some point in the future, the generic glibc version will change to another distro and break Ubuntu 20.04. With explicit Ubuntu 20.04 builds, they are guaranteed to work until we drop support for 20.04 when Canonical does the same.

You can opt into using the Ubuntu binaries when using ponyup by running:

```bash
ponyup default ubuntu20.04
```
