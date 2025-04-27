# Installing Pony

Prebuilt Pony binaries are available on a number of platforms. They are built using a very generic CPU instruction set and as such, will not provide maximum performance. If you need to get the best performance possible from your Pony program, we strongly recommend [building from source](BUILD.md).

All prebuilt releases are currently AMD64 only. If you want to install on different CPU architecture, you'll need to [build from source](BUILD.md).

Prebuilt Pony installations will use clang as the default C compiler and clang++ as the default C++ compiler. If you prefer to use different compilers, such as gcc and g++, these defaults can be overridden by setting the `$CC` and `$CXX` environment variables to your compiler of choice.

## Linux

Prebuilt Linux packages are available via [ponyup](https://github.com/ponylang/ponyup) for Glibc and musl libc based Linux distribution. You can install nightly builds as well as official releases using ponyup.

If you are running on a support Linux platform, ponyup should correctly select it so long as you have `cc` and `lsb_release` installed. If for some reason, the installation script can't identify your distribution, you can manually select your platform.

If we aren't creating packages for your distribution and you would like us to, please stop by the [release stream](https://ponylang.zulipchat.com/#narrow/stream/190364-release) in the [ponylang Zulip](https://ponylang.zulipchat.com) to discuss adding support. Please note, we are almost assuredly going to ask you to help support your distribution.

At the moment, we support all supported LTS Ubuntu versions and any distributions built on top of Ubuntu (like Linux Mint and Pop!_OS).

### Supported Glibc distributions

Currently, we have packages for the following Glibc based distributions:

- Fedora 41
- Linux Mint 19, 20, 21
- Pop!_OS 22.04, 24.04
- Ubuntu 22.04, 24.04

### Supported Alpine versions

Starting with Pony version 0.56.0, only Alpine 3.17 and later are supported. Pomyup will happily 0.56.0 on Alpine 3.16, but you won't be able to link programs. By the same token, installing version of Pony prior to 0.56.0 on Alpine 3.17 will also fail to link.

### Manually selecting your Linux platform

To manually set your platform if ponyup is unable to identify it:

```bash
ponyup default PLATFORM
```

where `PLATFORM` is from the table below

Distribution | PLATFORM String
--- | ---
Alpine | x86_64-linux-musl
Fedora 41 | x86_64-linux-fedora41
Linux Mint 21.x | x86_64-linux-ubuntu22.04
Pop!_OS 22.04 | x86_64-linux-ubuntu22.04
Pop!_OS 24.04 | x86_64-linux-ubuntu24.04
Ubuntu 22.04 | x86_64-linux-ubuntu22.04
Ubuntu 24.04 | x86_64-linux-ubuntu24.04

### Install the latest release

```bash

ponyup update ponyc release
```

### Additional requirements

All ponyc Linux installations need to have a C compiler such as clang installed. Compilers other than clang might work, but clang is the officially supported C compiler.

### Troubleshooting Glibc compatibility

Most Linux distributions are based on Glibc and all software for them must use the same version of Glibc. You might see an error like the following when trying to use ponyc:

```console
ponyc: /lib/x86_64-linux-gnu/libm.so.6: version `GLIBC_2.29' not found (required by ponyc)
```

If you get that error, it means that the Glibc we compiled ponyc with isn't compatible with your distribution. You've installed a ponyc build that isn't compatible with the C ABI on your distribution. You'll probably need to [build ponyc from source](BUILD.md). If you believe that you've installed a correct package for your distribution, please stop by the [beginner help stream](https://ponylang.zulipchat.com/#narrow/stream/189985-beginner-help) on the [ponylang Zulip](https://ponylang.zulipchat.com) to discuss what you are seeing.

## macOS

Prebuilt macOS packages are available for macOS via [ponyup](https://github.com/ponylang/ponyup). You can also install nightly builds using ponyup.

To install the most recent ponyc on macOS:

```bash
ponyup update ponyc release
```

## Windows

Windows users will need to install:

- Visual Studio 2022 or 2019 (available [here](https://www.visualstudio.com/vs/community/)) or the Microsoft C++ Build Tools (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/)).
  - Install the `Desktop Development with C++` workload, along with the latest `Windows 10 SDK (10.x.x.x) for Desktop` individual component.

Once you have installed the prerequisites, you can get prebuilt release or nightly builds via [ponyup](https://github.com/ponylang/ponyup).  To install the most recent ponyc on Windows:

```pwsh
ponyup update ponyc release
```

You can also download the latest ponyc release from [Cloudsmith](https://dl.cloudsmith.io/public/ponylang/releases/raw/versions/latest/ponyc-x86-64-pc-windows-msvc.zip). Unzip the release file in a convenient location, and you will find `ponyc.exe` in the `bin` directory. Following extraction, to make `ponyc.exe` globally available, add it to your `PATH` either by using Advanced System Settings->Environment Variables to extend `PATH` or by using the `setx` command, e.g. `setx PATH "%PATH%;<directory you unzipped to>\bin"`
