## Add prebuilt ponyc binaries for CentOS 8

Prebuilt nightly versions of ponyc are now available from our [Cloudsmith nightlies repo](https://cloudsmith.io/~ponylang/repos/nightlies/packages/). Release versions will be available in the [release repo](https://cloudsmith.io/~ponylang/repos/releases/packages/) starting with this release.

CentOS 8 releases were added at the request of Damon Kwok. If you are a Pony user and are looking for prebuilt releases for your distribution, open an issue and let us know. We'll do our best to add support for any Linux distribution version that is still supported by their creators.

# Fix building libs with Visual Studio 16.7

Visual Studio 16.7 introduced a change in C++20 compatibility that breaks building LLVM on Windows (#3634). We've added a patch for LLVM that fixes the issue.

## Add prebuilt ponyc binaries for Ubuntu 20.04

We've added prebuilt ponyc binaries specifically made to work on Ubuntu 20.04. At the time of this change, our generic glibc binaries are built on Ubuntu 20.04 and guaranteed to work on it. However, at some point in the future, the generic glibc version will change to another distro and break Ubuntu 20.04. With explicit Ubuntu 20.04 builds, they are guaranteed to work until we drop support for 20.04 when Canonical does the same.

You can opt into using the Ubuntu binaries when using ponyup by running:

```bash
ponyup default ubuntu20.04
```

