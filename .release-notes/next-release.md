## Add prebuilt ponyc binaries for Rocky Linux 8

Prebuilt nightly versions of ponyc are now available from our [Cloudsmith nightlies repo](https://cloudsmith.io/~ponylang/repos/nightlies/packages/). Release versions will be available in the [release repo](https://cloudsmith.io/~ponylang/repos/releases/packages/) starting with this release.

CentOS 8 releases were added as we currently support CentOS 8, but it will be discontinued in a few months and Rocky Linux provides a seamless transition path.

If you are a Pony user and are looking for prebuilt releases for your distribution, open an issue and let us know. We'll do our best to add support for any Linux distribution version that is still supported by their creators.

## Fix OOM error when running ponyc built with xcode 12.5

Changes in Big Sur and xcode as of 12.5 have lead to an out of memory error with ponyc. If pony's allocator is built with usage of VM_FLAGS_SUPERPAGE_SIZE_ANY on, it will run out of memory if xcode 12.5 was used to build.

VM_FLAGS_SUPERPAGE_SIZE_ANY isn't required on earlier versions. It does however, improve performance when allocating. Usage of VM_FLAGS_SUPERPAGE_SIZE_ANY has been removed as it also doesn't work on newer M1 based machines and thus, is on its way out in general.

## Fix API mismatch errors when linking pony programs on MacOS

We have been setting a minimal API version for ponyc that didn't match the LLVM value. This resulted in errors about how the link versions didn't match. The warnings caused no issues running programs, but did lead to confusion amongst new users. They were also annoying to look at all the time.

We did some testing and determined that there's no need to set the value as we can build ponyc and other pony programs on Big Sur and still use on earlier versions of MacOS.

There might be some issues that crop up in the future, but as far as we can tell, for normal ponyc MacOS usage, we dont need to set `macosx-version-min`.

## Fix broken `IsPrime` standard library feature

Any prime after 1321 wasn't being correctly identified.

## Prevent non-opaque structs from being used as behaviour parameters

When a non-opaque object is sent across actors, its type descriptor is used by the garbage collector in order to trace it. Since structs lack a type descriptor, using a `val` or `iso` struct as a parameter behaviour could lead to a runtime segfault. This also applies to tuple parameters where at least one element is a non-opaque struct.

This is a breaking change. Existing code will need to wrap struct parameters in classes or structs, or use structs with a `tag` capability. Where you previously had code like:

```pony
struct Foo

actor Main
  new create(env: Env) =>
    inspect(recover Foo end)

  be inspect(f: Foo iso) =>
    // Do something with f
```

you will now need

```pony
struct Foo

class Bar
  let f: Foo = Foo

actor Main
  new create(env: Env) =>
    inspect(recover Bar end)

  be inspect(wrap: Bar iso) =>
    // Do something with wrap.f
```

When using tuples with struct elements, you don't need to wrap the entire tuple. It is enough to wrap the struct elements.

## Update to LLVM 12.0.1

We've updated the LLVM used to build pony to LLVM 12.0.1 and in the process, we've dropped support for 32-bit ARM as supported platform.

We might bring 32-bit Arm back as a supported platform if the Arm fixes we need to do to get ponyc working on M1 also fix the current issues we have with 32-bit Arm with LLVM 12. The errors currently present the same so we assume that adding M1 support will bring 32-bit ARM along with it. If fixing the M1 issues doesn't fix 32-bit Arm issues then we'll have to make a decision about whether we bring back 32-bit Arm support.

