# Installing Pony

Prebuilt Pony binaries are available on a number of platforms. They are built using a very generic CPU instruction set and as such, will not provide maximum performance. If you need to get the best performance possible from your Pony program, we strongly recommend [building from source](BUILD.md).

## Linux

Prebuilt Linux packages are available via [ponyup](https://github.com/ponylang/ponyup) for Glibc and musl libc based Linux distribution. You can install nightly builds as well as official releases using ponyup.

If you are running on a supported Linux platform, ponyup should correctly select it so long as you have `cc` and `lsb_release` installed.

If we aren't creating packages for your distribution and you would like us to, please stop by the [release stream](https://ponylang.zulipchat.com/#narrow/stream/190364-release) in the [ponylang Zulip](https://ponylang.zulipchat.com) to discuss adding support. Please note, we are almost assuredly going to ask you to help support your distribution.

At the moment, we support all supported LTS Ubuntu versions and any distributions built on top of Ubuntu (like Linux Mint and Pop!_OS). Additionally, we support recent Alpine Linux releases.

### Install the latest release

```bash

ponyup update ponyc release
```

### Linux additional requirements

Install your distribution's libc development package — for example `libc6-dev` on Debian/Ubuntu-based systems or `musl-dev` on Alpine. Installing a C compiler package also works since it pulls these in as dependencies.

ponyc bundles its own linker, so you don't need a C or C++ compiler. What it links against is the system C library and the standard CRT startup objects from your libc-dev package.

## macOS

Prebuilt macOS packages are available for macOS via [ponyup](https://github.com/ponylang/ponyup). You can also install nightly builds using ponyup.

To install the most recent ponyc on macOS:

```bash
ponyup update ponyc release
```

### macOS additional requirements

ponyc bundles its own linker but uses the macOS SDK that ships with the Xcode Command Line Tools to find the system libraries it links your program against. If you don't already have them installed, run:

```bash
xcode-select --install
```

The full Xcode install also satisfies this requirement.

## Windows

To use ponyc on Windows, install one of:

- Visual Studio 2022 or 2019 (available [here](https://www.visualstudio.com/vs/community/))
- Microsoft C++ Build Tools (available [here](https://visualstudio.microsoft.com/visual-cpp-build-tools/))

When installing either, select the `Desktop Development with C++` workload along with the latest `Windows 11 SDK (11.x.x.x) for Desktop` individual component. ponyc bundles its own linker but links your program against the Windows SDK (UCRT, kernel32) and MSVC runtime libraries from this install.

Once you have installed the prerequisites, you can get prebuilt release or nightly builds via [ponyup](https://github.com/ponylang/ponyup).  To install the most recent ponyc on Windows:

```pwsh
ponyup update ponyc release
```

You can also download the latest ponyc release from [Cloudsmith](https://dl.cloudsmith.io/public/ponylang/releases/raw/versions/latest/ponyc-x86-64-pc-windows-msvc.zip). Unzip the release file in a convenient location, and you will find `ponyc.exe` in the `bin` directory. Following extraction, to make `ponyc.exe` globally available, add it to your `PATH` either by using Advanced System Settings->Environment Variables to extend `PATH` or by using the `setx` command, e.g. `setx PATH "%PATH%;<directory you unzipped to>\bin"`
