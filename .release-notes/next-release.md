## Fix calculating the number of CPU cores on FreeBSD/DragonFly

The number of CPU cores is used by Pony to limit the number of scheduler
threads (`--ponymaxthreads`) and also used as the default number of scheduler
threads.

Previously, `sysctl hw.ncpu` was incorrectly used to determine the number of
CPU cores on FreeBSD and DragonFly. On both systems `sysctl hw.ncpu` reports
the number of logical CPUs (hyperthreads) and NOT the number of physical CPU
cores.

This has been fixed to use `kern.smp.cores` on FreeBSD and
`hw.cpu_topology_core_ids` * `hw.cpu_topology_phys_ids` on DragonFly to
correctly determine the number of physical CPU cores.

This change only affects FreeBSD and DragonFly systems with hardware that
support hyperthreading. For instance, on a 4-core system with 2-way HT,
previously 8 scheduler threads would be used by default. After this change, the
number of scheduler threads on the same system is limited to 4 (one per CPU
core).

## Change supported FreeBSD version to 12.2

We only maintain support for a single version of FreeBSD at a time due to limited usage of Pony of FreeBSD. We've switched from testing and building releases for 12.1 to 12.2, We would have done this sooner, but we hadn't noticed that 12.2 had been released.

## Fix FFI declarations ignoring partial annotation

Previously, the compiler would ignore any `?` annotations on FFI declarations, which could result in the generated code lacking any guards for errors, causing the final program to abort at runtime. This change fixes the problem by ensuring that the compiler checks if FFI declarations specify a partial annotation.

## Fix building ponyc on DragonFly BSD

DragonFly BSD is lightly used as a supposed platform. It is also "unsupported" due to lack of CI. Over time, ponyc fell out of being able to be built on DragonFly. 

As of this release, you should be able to use ponyc on DragonFly again. However, do to the unsupported nature of the DragonFly port, it might break again in which case, updates from anyone using Pony on DragonFly will be needed.

## Create a standalone libponyc on Linux

It's possible to use libponyc from projects other than ponyc. Using the
library gives you access to all the various compiler functionality. However,
it is hard to use the library because, you need to link in the correct libstdc++,
libblake, and LLVM. Figuring out what is the correct version of each is very
difficult.

With this change, a new library `libponyc-standalone.a` will be created as
part of the build process.

Applications like the "in pony documentation generator" that we are working on will
be able to link to libponyc-standalone and pick up all it's required dependencies.

Windows, BSD, and macOS libraries are not created at this time but can be added
as needed in the future.

