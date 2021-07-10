## Add prebuilt ponyc binaries for Rocky Linux 8

Prebuilt nightly versions of ponyc are now available from our [Cloudsmith nightlies repo](https://cloudsmith.io/~ponylang/repos/nightlies/packages/). Release versions will be available in the [release repo](https://cloudsmith.io/~ponylang/repos/releases/packages/) starting with this release.

CentOS 8 releases were added as we currently support CentOS 8, but it will be discontinued in a few months and Rocky Linux provides a seamless transition path.

If you are a Pony user and are looking for prebuilt releases for your distribution, open an issue and let us know. We'll do our best to add support for any Linux distribution version that is still supported by their creators.

## Fix OOM error when running ponyc built with xcode 12.5

Changes in Big Sur and xcode as of 12.5 have lead to an out of memory error with ponyc. If pony's allocator is built with usage of VM_FLAGS_SUPERPAGE_SIZE_ANY on, it will run out of memory if xcode 12.5 was used to build.

VM_FLAGS_SUPERPAGE_SIZE_ANY isn't required on earlier versions. It does however, improve performance when allocating. Usage of VM_FLAGS_SUPERPAGE_SIZE_ANY has been removed as it also doesn't work on newer M1 based machines and thus, is on its way out in general.

