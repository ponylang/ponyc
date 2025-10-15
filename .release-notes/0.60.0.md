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

## Update nightly Pony musl Docker image to Alpine 3.21

We've updated our `ponylang/ponyc:alpine` image to be based on Alpine 3.21. Previously, we were using Alpine 3.20 as the base. The release images are still based on Alpine 3.20 for now.
## Stop building Windows container images

We've stopped building Windows container images as of Pony 0.60.0. As far as we know, the Windows images were not being used. We've simplified our CI by removing them.

## Stop building glibc based ponyc images

We have stopped building nightly and release ponyc Docker images that are based on glibc. The glibc based images that we were building were based on an ever changing version of Ubuntu (most recent LTS release). These images had minimal utility.

They could only be used to link for Linux distros that had the same libraries installed as the build environment. Whereas the musl images could be used to statically link using musl and create binaries that could be used on any Linux.

Given the lack of utility for a wide variety of people, we have stopped building the glibc based images and will only have a single nightly and release image going forward. Those images will be using musl via Alpine Linux.

If you were using the glibc based images, we can point you to the Dockerfiles we were using as they are quite straightfoward and should be easy for anyone to incorporate the creation of into their own CI process. You can find the Dockerfiles in the [pull request that removed them](https://github.com/ponylang/ponyc/pull/4747).

## Add Multiplatform Nightly Docker Images

We've added new nightly images with support multiple architectures. The images support both arm64 and amd64. The images will be available with the tag `nightly`. They will be available for GitHub Container Registry the same as our [other ponyc images](https://github.com/ponylang/ponyc/pkgs/container/ponyc).

The images are based on Alpine 3.21. Every few months, we will be updating the base image to a newer version of Alpine Linux. At the moment, we generally add support for new Alpine version about 2 times a year.

We will be dropping the `alpine` tag in favor of the new `nightly` tag. However, that isn't happening now but will in the not so distant future. You should switch any usage you have of those tags to `nightly` now to avoid disruption later.

