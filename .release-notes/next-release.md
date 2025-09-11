## Drop Ubuntu 20.04 support

Ubuntu 20.04 has reached its end of life date. We've dropped it as a supported platform. That means, we no longer test against it when doing CI and we no longer create prebuilt binaries for installation via `ponyup` for Ubuntu 20.04.

We will maintain best effort to keep Ubuntu 20.04 continuing to work for anyone who wants to use it and builds `ponyc` from source.

## Don't set ld.gold as the default Linux linker

Previously, we were setting ld.gold as our default linker on Linux. If you wanted to use a different linker, you had to override it using the ponyc option `--link-ldcmd`.

ld.gold was contributed to the GNU project many years ago by Google. Over the years, Google has moved away from supporting it and to investing in LLVM and other tools. ld.gold has no active maintainers at this point in time and has been removed from the most recent version of binutils.

It is no longer reasonable going forward to set gold as the default Linux linker as many Linux distributions will not have it easily available. We have switched to using the default Linux linker on the given system. On most systems, that is ld.bfd unless it has been configured otherwise. If you would like to use a different linker you can do the following as part of ponyc command line:

Use gold:

- `ponyc --link-ldcmd=gold`

Use mold:

- `ponyc --link-ldcmd=mold`

## Alpine 3.20 added as a supported platform

We've added Alpine 3.20 as a supported platform. We'll be building ponyc releases for it until it stops receiving security updates in 2026. At that point, we'll stop building releases for it.

## Alpine 3.21 added as a supported platform

We've added Alpine 3.21 as a supported platform. We'll be building ponyc releases for it until it stops receiving security updates in 2026. At that point, we'll stop building releases for it.

## Ubuntu 24.04 on arm64 added as a supported platform

We've added Ubuntu 24.04 on arm64 as a supported platform. We'll be building ponyc releases for it until it stops receiving security updates in 2029. At that point, we'll stop building releases for it.

## Alpine 3.21 on arm64 added as a supported platform

We've added Alpine 3.21 on arm64 as a supported platform. We'll be building ponyc releases for it until it stops receiving security updates in 2026. At that point, we'll stop building releases for it.
## Update to LLVM 18.1.8

We've updated the LLVM version used to build Pony to 18.1.8.

## Support building ponyc natively on arm64 Windows

We've added support for building Pony for arm64 Windows.

## Arm64 Windows added as a supported platform

We've added arm64 Windows as a supported platform. Builds for it are available in [Cloudsmith](https://cloudsmith.io/~ponylang/repos/). Corral and ponyup support will be coming soon.

## Make finding Visual Studio more robust

Ponyc will now ignore applications other than Visual Studio itself which use the Visual Studio installation system (e.g. SQL Server Management Studio) when looking for `link.exe`.

## Fix linking on Linux arm64 when using musl libc

On arm64-based Linux systems using musl libc, program linking could fail due to missing libgcc symbols. We've now added linking against libgcc on all arm-based Linux systems to resolve this issue.

