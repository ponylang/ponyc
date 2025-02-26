# Change Log

All notable changes to the Pony compiler and standard library will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/) and [Keep a CHANGELOG](http://keepachangelog.com/).

## [unreleased] - unreleased

### Fixed

- Don't duplicate match checks for inherited trait bodies ([PR #4628](https://github.com/ponylang/ponyc/pull/4628))
- Don't duplicate visibility tests for inherited trait bodies ([PR #4640](https://github.com/ponylang/ponyc/pull/4640))

### Added


### Changed


## [0.58.11] - 2025-02-22

### Fixed

- Fix use-after-free triggered by fast actor reaping when the cycle detector is active ([PR #4616](https://github.com/ponylang/ponyc/pull/4616))
- Fix incorrect printing of runtime stats ([PR #4620](https://github.com/ponylang/ponyc/pull/4620))
- Fix memory leak ([PR #4621](https://github.com/ponylang/ponyc/pull/4621))
- Fix being unable to compile programs that use runtime info on Windows ([PR #4625](https://github.com/ponylang/ponyc/pull/4625))

## [0.58.10] - 2025-02-01

### Fixed

- Make sure scheduler threads don't ACK the quiescence protocol CNF messages if they have an actor waiting to be run ([PR #4583](https://github.com/ponylang/ponyc/pull/4583))
- Apply default options for a CLI parent command when a sub command is parsed ([PR #4593](https://github.com/ponylang/ponyc/pull/4593))
- Fix compiler crash from `match` with extra parens around `let` in tuple ([PR #4595](https://github.com/ponylang/ponyc/pull/4595))
- Fix soundness problem when matching `iso` variables ([PR #4588](https://github.com/ponylang/ponyc/pull/4588))

### Changed

- Change stack_depth_t to size_t on OpenBSD ([PR #4575](https://github.com/ponylang/ponyc/pull/4575))

## [0.58.9] - 2024-12-29

### Fixed

- Fixed an issue that caused the `actor_pinning` documentation to not be built

## [0.58.8] - 2024-12-27

### Fixed

- Fix rare termination logic failures that could result in early shutdown ([PR #4556](https://github.com/ponylang/ponyc/pull/4556))

### Added

- Add Fedora 41 as a supported platform ([PR #4557](https://github.com/ponylang/ponyc/pull/4557))
- Add support for pinning actors to a dedicated scheduler thread ([PR #4547](https://github.com/ponylang/ponyc/pull/4547))

### Changed

- Drop Fedora 39 support ([PR #4558](https://github.com/ponylang/ponyc/pull/4558))
- Update Pony musl Docker images to Alpine 3.20 ([PR #4562](https://github.com/ponylang/ponyc/pull/4562))

## [0.58.7] - 2024-11-30

### Fixed

- Correctly find custom-built `llc` ([PR #4537](https://github.com/ponylang/ponyc/pull/4537))
- Fix buffer out of bounds access issue ([PR #4540](https://github.com/ponylang/ponyc/pull/4540))
- Fix bug in ASIO shutdown ([PR #4548](https://github.com/ponylang/ponyc/pull/4548))
- Fix early quiescence/termination bug ([PR #4550](https://github.com/ponylang/ponyc/pull/4550))

### Changed

- Recycle actor heap chunks after GC instead of returning to pool ([PR #4531](https://github.com/ponylang/ponyc/pull/4531))

## [0.58.6] - 2024-10-16

### Fixed

- Fix use after free bug in actor heap finalisation that can lead to a segfault ([PR #4522](https://github.com/ponylang/ponyc/pull/4522))
- Make heap small chunk size setting logic more precise/correct ([PR #4527](https://github.com/ponylang/ponyc/pull/4527))

## [0.58.5] - 2024-06-01

### Changed

- Update the base image for our ponyc images ([PR #4515](https://github.com/ponylang/ponyc/pull/4515))

## [0.58.4] - 2024-05-01

### Fixed

- Fix compiler crash ([PR #4505](https://github.com/ponylang/ponyc/pull/4505))
- Fix generation of invalid LLVM IR  ([PR #4506](https://github.com/ponylang/ponyc/pull/4506))

### Added

- Add prebuilt ponyc binaries for Ubuntu 24.04 ([PR #4508](https://github.com/ponylang/ponyc/pull/4508))

## [0.58.3] - 2024-03-30

### Fixed

- Fix bug in documentation generation ([PR #4502](https://github.com/ponylang/ponyc/pull/4502))

## [0.58.2] - 2024-02-24

### Fixed

- Fix for potential memory corruption in `Array.copy_to` ([PR #4490](https://github.com/ponylang/ponyc/pull/4490))
- Fix bug when serializing bare lambdas ([PR #4486](https://github.com/ponylang/ponyc/pull/4486))

### Added

- Add Fedora 39 as a supported platform ([PR #4485](https://github.com/ponylang/ponyc/pull/4485))
- Add MacOS on Apple Silicon as a supported platform ([PR #4487](https://github.com/ponylang/ponyc/pull/4487))
- Add constrained_types package to the standard library ([PR #4493](https://github.com/ponylang/ponyc/pull/4493))

## [0.58.1] - 2024-01-27

### Fixed

- Fix missing "runtime_info" package documentation ([PR #4476](https://github.com/ponylang/ponyc/pull/4476))
- Use the correct LLVM intrinsics for `powi` on *nix. ([PR #4481](https://github.com/ponylang/ponyc/pull/4481))

## [0.58.0] - 2023-11-24

### Changed

- Disallow `return` at the end of a `with` block ([PR #4467](https://github.com/ponylang/ponyc/pull/4467))
- Make the `verify` pass on by default ([PR #4036](https://github.com/ponylang/ponyc/pull/4036))

## [0.57.1] - 2023-10-29

### Fixed

- Fix compiling Pony programs on X86 MacOS when XCode 15 is the linker ([PR #4466](https://github.com/ponylang/ponyc/pull/4466))

## [0.57.0] - 2023-10-08

### Fixed

- Fix broken DTrace support ([PR #4453](https://github.com/ponylang/ponyc/pull/4453))
- Fix compilation error when building with pool_memalign in release mode ([PR #4455](https://github.com/ponylang/ponyc/pull/4455))
- Fix compiler bug that allows an unsafe data access pattern ([PR #4458](https://github.com/ponylang/ponyc/pull/4458))

### Changed

- Fix compiler bug that allows an unsafe data access pattern ([PR #4458](https://github.com/ponylang/ponyc/pull/4458))

## [0.56.2] - 2023-09-16

### Added

- "No op" release to get Windows release out

## [0.56.1] - 2023-09-16

### Fixed

- Fix "double socket close" issue with Windows version of TCPConnection ([PR #4437](https://github.com/ponylang/ponyc/pull/4437))

## [0.56.0] - 2023-08-30

### Fixed

- Avoid hangs in async pony_check properties when using actions ([PR #4405](https://github.com/ponylang/ponyc/pull/4405))

### Added

- Add macOS on Intel as fully supported platform ([PR #4390](https://github.com/ponylang/ponyc/pull/4390))

### Changed

- Drop support for Alpine versions prior to 3.17 ([PR #4407](https://github.com/ponylang/ponyc/pull/4407))
- Update Pony musl Docker images to Alpine 3.18 ([PR #4407](https://github.com/ponylang/ponyc/pull/4407))
- Drop FreeBSD as a supported platform ([PR #4382](https://github.com/ponylang/ponyc/pull/4382))
- Drop macOS on Apple Silicon as a fully supported platform ([PR #4403](https://github.com/ponylang/ponyc/pull/4403))

## [0.55.1] - 2023-08-16

### Fixed

- Fix broken linking when using a sanitizer ([PR #4393](https://github.com/ponylang/ponyc/pull/4393))
- Fix memory errors with some `--debug` program builds ([PR #4372](https://github.com/ponylang/ponyc/pull/4372))

### Changed

- Stop putting `stable` in ponyc Docker images ([PR #4353](https://github.com/ponylang/ponyc/pull/4353))
- Move heap ownership info from chunk to pagemap ([PR #4371](https://github.com/ponylang/ponyc/pull/4371))

## [0.55.0] - 2023-05-27

### Changed

- Change supported MacOS version from Monterey to Ventura ([PR #4349](https://github.com/ponylang/ponyc/pull/4349))
- Fix a possible resource leak with `with` blocks ([PR #4347](https://github.com/ponylang/ponyc/pull/4347))
- Drop Ubuntu 18.04 support ([PR #4351](https://github.com/ponylang/ponyc/pull/4351))

## [0.54.1] - 2023-04-12

### Fixed

- Fix bug in HeapToStack optimization pass ([PR #4341](https://github.com/ponylang/ponyc/pull/4341))

### Changed

- LLVM 15 ([PR #4327](https://github.com/ponylang/ponyc/pull/4327))

## [0.54.0] - 2023-02-27

### Fixed

- Remove ambiguity from "not safe to write" compiler error message ([PR #4299](https://github.com/ponylang/ponyc/pull/4299))
- Fix waiting on Windows to be sure I/O events can still come in. ([PR #4325](https://github.com/ponylang/ponyc/pull/4325))

### Added

- Create libponyc-standalone on MacOS ([PR #4303](https://github.com/ponylang/ponyc/pull/4303))
- Build ponyc-standalone.lib for windows ([PR #4307](https://github.com/ponylang/ponyc/pull/4307))

### Changed

- Update DTrace probes ([PR #4302](https://github.com/ponylang/ponyc/pull/4302))
- Stop building the "x86-64-unknown-linux-gnu" ponyc package ([PR #4312](https://github.com/ponylang/ponyc/pull/4312))
- Remove json package from the standard library ([PR #4323])(https://github.com/ponylang/ponyc/pull/4323)

## [0.53.0] - 2023-01-04

### Fixed

- Fix infinite loop in compiler ([PR #4293](https://github.com/ponylang/ponyc/pull/4293))
- Fix compiler segfault caused by infinite recursion ([PR #4292](https://github.com/ponylang/ponyc/pull/4292))
- Fix runtime segfault ([PR #4294](https://github.com/ponylang/ponyc/pull/4294))

### Changed

- Implement empty ranges RFC ([PR #4280](https://github.com/ponylang/ponyc/pull/4280))

## [0.52.5] - 2022-12-30

### Fixed

- Fix compiler crash introduced in #4283 ([PR #4288](https://github.com/ponylang/ponyc/pull/4288))

## [0.52.4] - 2022-12-29

### Fixed

- Fix an assert in `call.c` when checking an invalid argument for autorecover ([PR #4278](https://github.com/ponylang/ponyc/pull/4278))
- Fix an issue with infinite loops while typechecking some expressions ([PR #4274](https://github.com/ponylang/ponyc/pull/4274))
- Fix soundness bug introduced in Pony 0.51.2 ([PR #4283](https://github.com/ponylang/ponyc/pull/4283))

## [0.52.3] - 2022-12-16

### Fixed

- Fix segfault caused by unsafe garbage collection optimization ([PR #4256](https://github.com/ponylang/ponyc/pull/4256))
- Fix incorrectly implemented PR #4243 ([PR #4276](https://github.com/ponylang/ponyc/pull/4276))

### Changed

- Remove macOS on Intel as a supported platform ([PR #4270](https://github.com/ponylang/ponyc/pull/4270))

## [0.52.2] - 2022-12-01

### Fixed

- Fix multiple races within actor/cycle detector interactions ([PR #4251](https://github.com/ponylang/ponyc/pull/4251))
- Fix crash when calling `(this.)create()` ([PR #4263](https://github.com/ponylang/ponyc/pull/4263))

### Added

- Don't include "home file" in documentation search ([PR #4243](https://github.com/ponylang/ponyc/pull/4243))
- Add support for erase left and erase line in term.ANSI ([PR #4246](https://github.com/ponylang/ponyc/pull/4246))

### Changed

- Improve TCP backpressure handling on Windows ([PR #4252](https://github.com/ponylang/ponyc/pull/4252))
- Update supported FreeBSD to 13.1 ([PR #4185](https://github.com/ponylang/ponyc/pull/4185))
- Drop Rocky 8 support ([PR #4262](https://github.com/ponylang/ponyc/pull/4262))

## [0.52.1] - 2022-11-14

### Added

- Update docgen to generate source files that are ignored ([PR #4239](https://github.com/ponylang/ponyc/pull/4239))

## [0.52.0] - 2022-11-10

### Fixed

- Avoid fairly easy to trigger overflow in Windows Time.nanos code ([PR #4227](https://github.com/ponylang/ponyc/pull/4227))
- Fix incorrect interaction between String/Array reserve and Pointer realloc ([PR #4223](https://github.com/ponylang/ponyc/pull/4223))
- Fix broken documentation generation on Windows ([PR #4226](https://github.com/ponylang/ponyc/pull/4226))

### Changed

- Sort package types in documentation ([PR #4228](https://github.com/ponylang/ponyc/pull/4228))
- Adapt documentation generation to use the mkdocs material theme ([PR #4226](https://github.com/ponylang/ponyc/pull/4226)

## [0.51.4] - 2022-10-29

### Fixed

- Fix broken readline package ([PR #4199](https://github.com/ponylang/ponyc/pull/4199))

## [0.51.3] - 2022-10-02

### Fixed

- Fix bug in `StdStream.print` ([PR #4180](https://github.com/ponylang/ponyc/pull/4180))
- Fix identity comparison check with desugared creations ([PR #4182](https://github.com/ponylang/ponyc/pull/4182))

## [0.51.2] - 2022-08-26

### Fixed

- Fix for crash when methods with default private type params in remote packages are called  ([PR #4167](https://github.com/ponylang/ponyc/pull/4167))
- Fix incorrect atomics usage ([PR #4159](https://github.com/ponylang/ponyc/pull/4159))

### Added

- Auto-recover constructor expressions ([PR #4124](https://github.com/ponylang/ponyc/pull/4124))
- Support for RISC-V ([PR #3435](https://github.com/ponylang/ponyc/pull/3435))
- Enhance runtime stats tracking ([PR #4144](https://github.com/ponylang/ponyc/pull/4144))

## [0.51.1] - 2022-06-29

### Fixed

- Fix String.f32 and String.f64 errors with non null terminated strings ([PR #4132](https://github.com/ponylang/ponyc/pull/4132))
- Fix for infinite Ranges ([PR #4127](https://github.com/ponylang/ponyc/pull/4127))
- Ensure reachability of types returned by FFI calls ([PR #4149](https://github.com/ponylang/ponyc/pull/4149))
- Support void* (Pointer[None]) parameters in bare lambdas and functions ([PR #4152](https://github.com/ponylang/ponyc/pull/4152))

### Added

- Avoid clearing chunks at start of GC ([PR #4143](https://github.com/ponylang/ponyc/pull/4143))
- Systematic testing for the runtime ([PR #4140](https://github.com/ponylang/ponyc/pull/4140))

### Changed

- Update to basing musl images off of Alpine 3.16 ([PR #4139](https://github.com/ponylang/ponyc/pull/4139))

## [0.51.0] - 2022-05-29

### Fixed

- Disable incorrect runtime assert for ASIO thread shutdown. ([PR #4122](https://github.com/ponylang/ponyc/pull/4122))

### Added

- Add prebuilt ponyc releases on MacOS for Apple Silicon ([PR #4119](https://github.com/ponylang/ponyc/pull/4119))

### Changed

- Update base image for glibc Linux docker images ([PR #4100](https://github.com/ponylang/ponyc/pull/4100))
- Update LLVM to 14.0.3 ([PR #4055](https://github.com/ponylang/ponyc/pull/4055))
- Don't use "mostly debug runtime" with release configurations ([PR #4112](https://github.com/ponylang/ponyc/pull/4112))

## [0.50.0] - 2022-04-30

### Fixed

- Fix crash with exhaustive match and generics ([PR #4057](https://github.com/ponylang/ponyc/pull/4057))
- Fix parameter names not being checked ([PR #4061](https://github.com/ponylang/ponyc/pull/4061))
- Fix compiler crash in HeapToStack optimization pass ([PR #4067](https://github.com/ponylang/ponyc/pull/4067))
- Strengthen the ordering for some atomic operations ([PR #4083](https://github.com/ponylang/ponyc/pull/4083))
- Fix a runtime fault for Windows IOCP w/ memtrack messages. ([PR #4094](https://github.com/ponylang/ponyc/pull/4094))

### Added

- Allow to override the return type of FFI functions ([PR #4060](https://github.com/ponylang/ponyc/pull/4060))
- Add prebuilt ponyc releases for Ubuntu 22.04 ([PR #4097](https://github.com/ponylang/ponyc/pull/4097))

### Changed

- Don't allow FFI calls in default methods or behaviors ([PR #4065](https://github.com/ponylang/ponyc/pull/4065))

## [0.49.1] - 2022-03-13

### Fixed

- Ban unstable variables ([PR #4018](https://github.com/ponylang/ponyc/pull/4018))

## [0.49.0] - 2022-02-26

### Fixed

- Add workaround for compiler assertion failure in Promise.flatten_next ([PR #3991](https://github.com/ponylang/ponyc/pull/3991))
- Take exhaustive match into account when checking for field initialization ([PR #4006](https://github.com/ponylang/ponyc/pull/4006))
- Fix compiler crash related to using tuples as a generic constraint ([PR #4005](https://github.com/ponylang/ponyc/pull/4005))
- Fix incorrect "field not initialized" error with while/else ([PR #4009](https://github.com/ponylang/ponyc/pull/4009))
- Fix incorrect "field not initialized" error with try/else ([PR #4011](https://github.com/ponylang/ponyc/pull/4011))
- Fix compiler crash related to using tuples in a union as a generic constraint ([PR #4017](https://github.com/ponylang/ponyc/pull/4017))
- Fix incorrect code returned by ANSI.erase ([PR #4022](https://github.com/ponylang/ponyc/pull/4022))
- Fix the signature of Iter's map_stateful/map to not require ephemerals ([PR #4026](https://github.com/ponylang/ponyc/pull/4026))
- Use symbol table from definition scope when looking up references from default method bodies ([PR #4027](https://github.com/ponylang/ponyc/pull/4027))
- Fix LLVM IR verification error found via PonyCheck ([PR #4039](https://github.com/ponylang/ponyc/pull/4039))
- Fix failed initialization bug in Process Monitor ([PR #4043](https://github.com/ponylang/ponyc/pull/4043))

### Added

- Add LeastCommonMultiple and GreatestCommonDivisor to math package ([PR #4001](https://github.com/ponylang/ponyc/pull/4001))
- Add PonyCheck to standard library ([PR #4034](https://github.com/ponylang/ponyc/pull/4034))

### Changed

- Reimplement `with` ([PR #4024](https://github.com/ponylang/ponyc/pull/4024))
- Remove `logger` package from the standard library ([PR #4035](https://github.com/ponylang/ponyc/pull/4035))
- Rename `ponybench` to match standard naming conventions. ([PR #4033](https://github.com/ponylang/ponyc/pull/4033))
- Change the standard library pattern for object capabilities ([PR #4031](https://github.com/ponylang/ponyc/pull/4031))
- Update Windows version used as the base for Windows images ([PR #4044](https://github.com/ponylang/ponyc/pull/4044))
- Rename `ponytest` package to conform to naming standards ([PR #4032](https://github.com/ponylang/ponyc/pull/4032))

## [0.48.0] - 2022-02-08

### Fixed

- Fix runtime crash when tracing class iso containing struct val ([PR #3993](https://github.com/ponylang/ponyc/pull/3993))

### Added

- Add a pony primitive that exposes scheduler information ([PR #3984](https://github.com/ponylang/ponyc/pull/3984))
- Expose additional scheduler info via the runtime_info package ([PR #3988](https://github.com/ponylang/ponyc/pull/3988))
- Add additional methods to itertools ([PR #3992](https://github.com/ponylang/ponyc/pull/3992))

### Changed

- Stop creating prebuilt ponyc releases for Ubuntu 21.04 ([PR #3990](https://github.com/ponylang/ponyc/pull/3990))
- Revert "prevent non-opaque structs from being used as behaviour parameters" ([PR #3995](https://github.com/ponylang/ponyc/pull/3995))
- Update LLVM to 13.0.1 ([PR #3994](https://github.com/ponylang/ponyc/pull/3994))
- Remove out parameter from `pony_os_stdin_read` ([PR #4000](https://github.com/ponylang/ponyc/pull/4000))

## [0.47.0] - 2022-02-02

### Fixed

- Fix return checking in behaviours and constructors ([PR #3971](https://github.com/ponylang/ponyc/pull/3971))
- Fix issue that could lead to a muted actor being run ([PR #3974](https://github.com/ponylang/ponyc/pull/3974))
- Fix loophole that allowed interfaces to be used to violate encapsulation ([PR #3973](https://github.com/ponylang/ponyc/pull/3973))
- Fix compiler assertion failure when assigning error to a variable ([PR #3980](https://github.com/ponylang/ponyc/pull/3980))

### Added

- Add "nodoc" annotation ([PR #3978](https://github.com/ponylang/ponyc/pull/3978))

### Changed

- Remove simplebuiltin compiler option ([PR #3965](https://github.com/ponylang/ponyc/pull/3965))
- Remove library mode option from ponyc ([PR #3975](https://github.com/ponylang/ponyc/pull/3975))
- Change `builtin/AsioEventNotify` from an interface to a trait ([PR #3973](https://github.com/ponylang/ponyc/pull/3973))
- Don't allow interfaces to have private methods ([PR #3973](https://github.com/ponylang/ponyc/pull/3973))
- Remove hack that prevented documentation generation for "test classes" ([PR #3978](https://github.com/ponylang/ponyc/pull/3978))

## [0.46.0] - 2022-01-16

### Changed

- Stop creating CentOS 8 prebuilt ponyc releases ([PR #3955](https://github.com/ponylang/ponyc/pull/3955))
- Hide C header implementation details related to actor pad size. ([PR #3960](https://github.com/ponylang/ponyc/pull/3960))
- Change type of Env.root to AmbientAuth ([PR #3962](https://github.com/ponylang/ponyc/pull/3962))

## [0.45.2] - 2021-12-31

### Fixed

- Clarify wording for some subtyping errors ([PR #3933](https://github.com/ponylang/ponyc/pull/3933))
- Fix inability to fully build pony on Raspberry PI 4's with 64-bit Raspbian ([PR #3949](https://github.com/ponylang/ponyc/pull/3949))

### Added

- Add build instructions for 64-bit Raspbian ([PR #3880](https://github.com/ponylang/ponyc/pull/3880))

## [0.45.1] - 2021-12-02

### Fixed

- Fix underlying time source for Time.nanos() on macOS ([PR #3921](https://github.com/ponylang/ponyc/pull/3921))
- Fix `cli` package from mangling option arguments with equal signs ([PR #3925](https://github.com/ponylang/ponyc/pull/3925))

## [0.45.0] - 2021-11-01

### Fixed

- Fix erratic cycle detector triggering on some Arm systems ([PR #3854](https://github.com/ponylang/ponyc/pull/3854))
- Fix non-release build crashes on Arm ([PR #3860](https://github.com/ponylang/ponyc/pull/3860))
- Fix major source of runtime instability on non-x86 based platforms ([PR #3871](https://github.com/ponylang/ponyc/pull/3871))
- Fix segfaults with debug mode code on 64-bit Arm ([PR #3875](https://github.com/ponylang/ponyc/pull/3875))
- Fix incorrect version in nightly ponyc builds ([PR #3895](https://github.com/ponylang/ponyc/pull/3895))

### Added

- Add Ubuntu 21.04 nightly builds and releases builds ([PR #3866](https://github.com/ponylang/ponyc/pull/3866))
- Add 64-bit Arm (Graviton) as a supported platform ([PR #3876](https://github.com/ponylang/ponyc/pull/3876))
- Add build instructions for 32-bit Raspbian ([PR #3879](https://github.com/ponylang/ponyc/pull/3879))
- Add Apple Silicon as a supported platform ([PR #3883](https://github.com/ponylang/ponyc/pull/3883))

### Changed

- Remove options package ([PR #3844](https://github.com/ponylang/ponyc/pull/3844))
- Update to LLVM 13.0.0 ([PR #3837](https://github.com/ponylang/ponyc/pull/3837))

## [0.44.0] - 2021-09-03

### Fixed

- Fix a compile-time crash related to Pony-specific optimizations. ([PR #3831](https://github.com/ponylang/ponyc/pull/3831))

### Changed

- Update FilePath constructors to allow a non-partial way to create a FilePath ([PR #3819](https://github.com/ponylang/ponyc/pull/3819))

## [0.43.2] - 2021-08-28

### Fixed

- Clean up child process exits on Windows ([PR #3817](https://github.com/ponylang/ponyc/pull/3817))
- Cleanup and fixes for Windows sockets ([PR #3816](https://github.com/ponylang/ponyc/pull/3816))
- Stop standalone libponyc from needing zlib ([PR #3827](https://github.com/ponylang/ponyc/pull/3827))

## [0.43.1] - 2021-08-03

### Fixed

- Fixed Makefile for FreeBSD ([PR #3808](https://github.com/ponylang/ponyc/pull/3808))

### Added

- Add FileMode.u32 ([PR #3809](https://github.com/ponylang/ponyc/pull/3809))
- Add FileMode.u32 ([PR #3810](https://github.com/ponylang/ponyc/pull/3810))
- Make working with promises of promises easier  ([PR #3813](https://github.com/ponylang/ponyc/pull/3813))

## [0.43.0] - 2021-07-14

### Fixed

- Fix OOM on MacOS when using xcode 12.5 ([PR #3793](https://github.com/ponylang/ponyc/pull/3793))
- Fix MacOS version mismatch warnings when linking Pony programs ([PR #3798](https://github.com/ponylang/ponyc/pull/3798))
- Fix the calculation of "is prime" for numbers after 1321. ([PR #3799](https://github.com/ponylang/ponyc/pull/3799))
- Prevent non-opaque structs from being used as behaviour parameters ([PR #3781](https://github.com/ponylang/ponyc/pull/3781))

### Added

- Add support for prebuilt Rocky Linux versions ([PR #3783](https://github.com/ponylang/ponyc/pull/3783))

### Changed

- Update to LLVM 12.0.1 ([PR #3745](https://github.com/ponylang/ponyc/pull/3745))

## [0.42.0] - 2021-07-07

### Fixed

- Fix bug where Flags.remove could set flags in addition to unsetting them ([PR #3777](https://github.com/ponylang/ponyc/pull/3777))

### Added

- Allow Flags instances to be created with a set bit encoding ([PR #3778](https://github.com/ponylang/ponyc/pull/3778))

### Changed

- Don't allow PONYPATH to override standard library ([PR #3780](https://github.com/ponylang/ponyc/pull/3780))

## [0.41.2] - 2021-06-29

### Fixed

- Fix "iftype" expressions not being usable in lambdas or object literals ([PR #3763](https://github.com/ponylang/ponyc/pull/3763))
- Fix code generation for variadic FFI functions on arm64 ([PR #3768](https://github.com/ponylang/ponyc/pull/3768))

## [0.41.1] - 2021-05-22

### Fixed

- Fix NullablePointer type constraint check being omitted in FFI declarations ([PR #3758](https://github.com/ponylang/ponyc/pull/3758))

## [0.41.0] - 2021-05-07

### Fixed

- Change to Steed's model of subtyping ([PR #3643](https://github.com/ponylang/ponyc/pull/3643))
- Fix memory corruption with Array.chop and String.chop ([PR #3755](https://github.com/ponylang/ponyc/pull/3755))

### Changed

- Improve error message for match on structs ([PR #3746](https://github.com/ponylang/ponyc/pull/3746))
- RFC 68: Mandatory FFI declarations ([PR #3739](https://github.com/ponylang/ponyc/pull/3739))
- Change return type of String.add to String iso^ ([PR #3752](https://github.com/ponylang/ponyc/pull/3752))
- Improve error message on destructuring of non-tuple types ([PR #3753](https://github.com/ponylang/ponyc/pull/3753))

## [0.40.0] - 2021-05-01

### Fixed

- Use built-in offset argument to cpointer ([PR #3741](https://github.com/ponylang/ponyc/pull/3741))

### Added

- Add `IsPrime` checker to `math` package ([PR #3738](https://github.com/ponylang/ponyc/pull/3738))

### Changed

- Change supported FreeBSD to 13.0 ([PR #3743](https://github.com/ponylang/ponyc/pull/3743))

## [0.39.1] - 2021-03-29

### Fixed

- Fix compiler crash related to type parameter references ([PR #3725](https://github.com/ponylang/ponyc/pull/3725))
- Fix early pipe shutdown with Windows ProcessMonitor ([PR #3726](https://github.com/ponylang/ponyc/pull/3726))
- Fix literal inference through partial function ([PR #3729](https://github.com/ponylang/ponyc/pull/3729))

## [0.39.0] - 2021-02-27

### Fixed

- Fix calculation of # CPU cores (FreeBSD/DragonFly) ([PR #3707](https://github.com/ponylang/ponyc/pull/3707))
- Fix partial FFI declarations ignoring partial annotation ([PR #3713](https://github.com/ponylang/ponyc/pull/3713))
- Fix building ponyc on DragonFly BSD ([PR #3676](https://github.com/ponylang/ponyc/pull/3676))
- Fix symbol table patching for overriding default methods ([PR #3719](https://github.com/ponylang/ponyc/pull/3719))
- Fix tuple related compiler segfaults ([PR #3723](https://github.com/ponylang/ponyc/pull/3723))

### Added

- Create a standalone libponyc on Linux ([PR #3716](https://github.com/ponylang/ponyc/pull/3716))

### Changed

- Update supported FreeBSD to 12.2 ([PR #3706](https://github.com/ponylang/ponyc/pull/3706))

## [0.38.3] - 2021-01-29

### Fixed

- Fix memory safety problem with Array.from_cpointer ([PR #3675](https://github.com/ponylang/ponyc/pull/3675))
- Fix bad package names in generated documentation ([PR #3700](https://github.com/ponylang/ponyc/pull/3700))

## [0.38.2] - 2020-12-26

### Fixed

- Fix race conditions that can lead to a segfault ([PR #3667](https://github.com/ponylang/ponyc/pull/3667))
- Fix compiler crash when an if block ends with an assignment that has no result value. ([PR #3670](https://github.com/ponylang/ponyc/pull/3670))
- Fix link errors on macOS Big Sur ([PR #3686](https://github.com/ponylang/ponyc/pull/3686))
- Fix unhandled null pointer returning from os_addrinfo_intern ([PR #3687](https://github.com/ponylang/ponyc/pull/3687))

## [0.38.1] - 2020-09-26

### Fixed

- Fix race condition in cycle detector block sent handling ([PR #3666](https://github.com/ponylang/ponyc/pull/3666))

## [0.38.0] - 2020-09-24

### Fixed

- Fix build failure under GCC 10.2 ([PR #3630](https://github.com/ponylang/ponyc/pull/3630))
- Fix building libs with Visual Studio 16.7 ([PR #3635](https://github.com/ponylang/ponyc/pull/3635))
- Fix missing Makefile lines to re-enable multiple `use=` options ([PR #3637](https://github.com/ponylang/ponyc/pull/3637))
- Consistent handling of  function calls in consume expressions ([PR #3647](https://github.com/ponylang/ponyc/pull/3647))
- Speed up cycle detector reaping some actors ([PR #3649](https://github.com/ponylang/ponyc/pull/3649))
- Prevent compiler crashes on certain `consume` expressions ([PR #3650](https://github.com/ponylang/ponyc/pull/3650))
- Fix soundness problem with Array.chop ([PR #3657](https://github.com/ponylang/ponyc/pull/3657))
- Allow for building on DragonFlyBSD again ([PR #3654](https://github.com/ponylang/ponyc/pull/3654))

### Added

- Add prebuilt ponyc binaries for CentOS 8 ([PR #3629](https://github.com/ponylang/ponyc/pull/3629))
- Add prebuilt ponyc binaries for Ubuntu 20.04 ([PR #3632](https://github.com/ponylang/ponyc/pull/3632))
- Improvements to garbage collection of short-lived actors ([PR #3653](https://github.com/ponylang/ponyc/pull/3653))

### Changed

- Make Range.next() partial ([PR #3639](https://github.com/ponylang/ponyc/pull/3639))

## [0.37.0] - 2020-08-28

### Fixed

- Fix unsound return types being allowed for autorecover ([PR #3595](https://github.com/ponylang/ponyc/pull/3595))
- Fix compile error with GCC 8.3 ([PR #3618](https://github.com/ponylang/ponyc/pull/3618))
- Always allow field access for objects defined inside a recover block ([PR #3606](https://github.com/ponylang/ponyc/pull/3606))

### Changed

- Update to basing musl images off of Alpine 3.12 ([PR #3609](https://github.com/ponylang/ponyc/pull/3609))
- Revert "Implement RFC0067" ([PR #3619](https://github.com/ponylang/ponyc/pull/3619))

## [0.36.0] - 2020-07-31

### Fixed

- Compile fix for the latest MSVC compiler (MSVC 19.26.28806.0, from Visual Studio 16.6.2) ([PR #3576](https://github.com/ponylang/ponyc/pull/3576))
- Fix with expressions using tuple destructuring. ([PR #3586](https://github.com/ponylang/ponyc/pull/3586))
- Fix typeparam check endless recursion ([PR #3589](https://github.com/ponylang/ponyc/pull/3589))
- Force removing read-only files on Windows with `FilePath.remove()` ([PR #3588](https://github.com/ponylang/ponyc/pull/3588))

### Added

- Added vs2019 preview support for building ponyc ([PR #3587](https://github.com/ponylang/ponyc/pull/3587))
- Implement RFC 66: Iter maybe ([PR #3603](https://github.com/ponylang/ponyc/pull/3603))

### Changed

- Change trn->trn to box to fix a soundness hole ([PR #3591](https://github.com/ponylang/ponyc/pull/3591))
- Add LogLevel as argument to LogFormatter ([PR #3597](https://github.com/ponylang/ponyc/pull/3597))

## [0.35.1] - 2020-05-13

### Fixed

- Fix incorrect Windows process waiting ([PR #3559](https://github.com/ponylang/ponyc/pull/3559))

### Changed

- Rename FreeBSD artifacts ([PR #3556](https://github.com/ponylang/ponyc/pull/3556))

## [0.35.0] - 2020-05-11

### Fixed

- Fix CommandParser incorrectly handling multiple end-of-option delimiters ([PR #3541](https://github.com/ponylang/ponyc/pull/3541))
- Correctly report process termination status in ProcessNotify.dispose() ([PR #3419](https://github.com/ponylang/ponyc/pull/3419))
- Ensure non-blocking process wait and correctly report process exit status ([PR #3419](https://github.com/ponylang/ponyc/pull/3419))
- Fix atomics usage related to actor muting for ARM ([PR #3552](https://github.com/ponylang/ponyc/pull/3552))

### Added

- Add Ubuntu18.04 builds ([PR #3545](https://github.com/ponylang/ponyc/pull/3545))

### Changed

- Ensure that waiting on a child process when using ProcessMonitor is non-blocking ([PR #3419](https://github.com/ponylang/ponyc/pull/3419))

## [0.34.1] - 2020-05-07

### Fixed

- Fix unneeded dependency on Linux glibc systems ([PR #3538](https://github.com/ponylang/ponyc/pull/3538))

## [0.34.0] - 2020-05-03

### Added

- Build Docker images for windows builds ([PR #3492](https://github.com/ponylang/ponyc/pull/3492))
- Added support for VS Preview ([PR #3487](https://github.com/ponylang/ponyc/pull/3487))
- Add nightly FreeBSD 12.1 builds ([PR #3502](https://github.com/ponylang/ponyc/pull/3502))
- Add OSSockOpt constants added by Linux 5.1 kernel ([PR #3515](https://github.com/ponylang/ponyc/pull/3515))
- Add prebuilt FreeBSD 12.1 builds for releases ([PR #3525](https://github.com/ponylang/ponyc/pull/3525))

### Changed

- Build PonyC using CMake ([PR #3234](https://github.com/ponylang/ponyc/pull/3234))
- Update  supported FreeBSD version to 12.1 ([PR #3495](https://github.com/ponylang/ponyc/pull/3495))
- Make clang our default compiler on Linux, macOS, and FreeBSD ([PR #3506](https://github.com/ponylang/ponyc/pull/3506))
- schedule the cycle detector with higher priority using the inject queue ([PR #3507](https://github.com/ponylang/ponyc/pull/3507))
- Update glibc Docker image based to Ubuntu 20 ([PR #3522](https://github.com/ponylang/ponyc/pull/3522))
- Change supported Ubuntu version to Ubuntu 20 ([PR #3522](https://github.com/ponylang/ponyc/pull/3522))
- Let processmonitor chdir before exec ([PR #3530](https://github.com/ponylang/ponyc/pull/3530))
- Removed unused Unsupported error from ProcessMonitor([PR #3530](https://github.com/ponylang/ponyc/pull/3530))
- Update ProcessMonitor errors to contain error messages ([PR #3532](https://github.com/ponylang/ponyc/pull/3532))

## [0.33.2] - 2020-02-03

### Fixed

- fix cli issue when providing --help=false. ([PR #3442](https://github.com/ponylang/ponyc/pull/3442))
- Fix linker error when creating symlinks on Windows ([PR #3444](https://github.com/ponylang/ponyc/pull/3444))
- Fix "not match" and "not if" causing a syntax error ([PR #3449](https://github.com/ponylang/ponyc/pull/3449))

### Added

- LLVM 9.0.x support ([PR #3320](https://github.com/ponylang/ponyc/pull/3320))

### Changed

- Better error message for check_receiver_cap ([PR #3450](https://github.com/ponylang/ponyc/pull/3450))
- Improved error for undefined but used left side of declarations ([PR #3451](https://github.com/ponylang/ponyc/pull/3451))

## [0.33.1] - 2019-12-13

### Fixed

- Fix building ponyc with clang on Ubuntu ([PR #3378](https://github.com/ponylang/ponyc/pull/3378))
- Fix error using latest VS2019 to build ponyc ([PR #3369](https://github.com/ponylang/ponyc/pull/3369))

### Changed

- Update default LLVM 7.1.0 ([PR #3377](https://github.com/ponylang/ponyc/pull/3377))

## [0.33.0] - 2019-11-01

### Fixed

- Building ponyc with GCC 8+ ([PR #3345](https://github.com/ponylang/ponyc/pull/3345))

### Added

- Allow programmatic override of the default runtime options ([PR #3342](https://github.com/ponylang/ponyc/pull/3342))

### Changed

- `--ponythreads` has been renamed to `--ponymaxthreads` ([PR #3334](https://github.com/ponylang/ponyc/pull/3334))
- All `--pony*` options that accept a value, will be checked for minimal values ([PR #3303](https://github.com/ponylang/ponyc/pull/3317))
- Default to statically linking LLVM into ponyc ([PR #3355](https://github.com/ponylang/ponyc/pull/3355))

## [0.32.0] - 2019-09-29

### Added

- Allow fields to be `consume`d (sometimes) ([PR #3304](https://github.com/ponylang/ponyc/pull/3304))
- `--ponynoscale` option ([PR #3303](https://github.com/ponylang/ponyc/pull/3303))
- `--ponyhelp` option to compiled program ([PR #3312](https://github.com/ponylang/ponyc/pull/3312))

### Changed

- Rename MaybePointer to NullablePointer ([PR #3293](https://github.com/ponylang/ponyc/pull/3293))
- `--ponyminthreads` option can't be larger than `--ponythreads` ([PR #3303](https://github.com/ponylang/ponyc/pull/3303))
- `--ponythreads` option can't be larger than cores available ([PR #3303](https://github.com/ponylang/ponyc/pull/3303))

## [0.31.0] - 2019-08-31

### Fixed

- Fix static linking issue by changing the link order ([PR #3259](https://github.com/ponylang/ponyc/pull/3259))

### Added

- Add `--link-ldcmd` command line argument for overriding the `ld` command used for linking ([PR #3259](https://github.com/ponylang/ponyc/pull/3259))
- Make builds with `musl` on `glibc` systems possible ([PR #3263](https://github.com/ponylang/ponyc/pull/3263))
- Add `proxy_via(destination_host, destination_service)` to `TCPConnectionNotify` to allow TCP handlers to change the hostname & service from a TCPConnectionNotify before connecting ([PR #3230](https://github.com/ponylang/ponyc/pull/3230))
- Add `add` and `sub` to `collections/persistent/Map` ([PR #3275](https://github.com/ponylang/ponyc/pull/3275))

### Changed

- Remove unnecessary argument to `Map.sub` ([PR #3275](https://github.com/ponylang/ponyc/pull/3275))
- No longer supply AppImage as a release format ([PR #3288](https://github.com/ponylang/ponyc/pull/3288))

## [0.30.0] - 2019-07-27

### Fixed

- Fix `which dtrace` path check ([PR #3229](https://github.com/ponylang/ponyc/pull/3229))
- Fix segfault due to Cycle Detector viewref inconsistency ([PR #3254](https://github.com/ponylang/ponyc/pull/3254))

### Changed

- Make Map insertion functions total ([PR #3203](https://github.com/ponylang/ponyc/pull/3203))
- Stop building Tumbleweed packages for releases ([PR #3228](https://github.com/ponylang/ponyc/pull/3228))
- Stop creating Debian Buster releases ([PR #3227](https://github.com/ponylang/ponyc/pull/3227))
- Remove `glob` package from standard library ([PR #3220](https://github.com/ponylang/ponyc/pull/3220))
- Remove `regex` package from standard library ([PR #3218](https://github.com/ponylang/ponyc/pull/3218))
- Remove `crypto` package from standard library ([PR #3225](https://github.com/ponylang/ponyc/pull/3225))
- Remove `net/ssl` package from standard library ([PR #3225](https://github.com/ponylang/ponyc/pull/3225))

## [0.29.0] - 2019-07-06

### Fixed

- Do not permit leading zeros in JSON numbers ([PR #3167](https://github.com/ponylang/ponyc/pull/3167))
- Make reading via TCPConnection re-entrant safe ([PR #3175](https://github.com/ponylang/ponyc/pull/3175))
- Cleanup TCPConnection GC-safety mechanism for writev buffers ([PR #3177](https://github.com/ponylang/ponyc/pull/3177))
- Add SSL tests and fix some SSL related bugs ([PR #3174](https://github.com/ponylang/ponyc/pull/3174))
- Fix lib/llvm to support MacOS ([PR #3181](https://github.com/ponylang/ponyc/pull/3181))
- Close Denial of Service issue with TCPConnection.expect ([PR #3197](https://github.com/ponylang/ponyc/pull/3197))
- Fix return type checking to allow aliasing for non-ephemeral return types. ([PR #3201](https://github.com/ponylang/ponyc/pull/3201))

### Added

- Allow use of OpenSSL 1.1.1 when building Pony ([PR #3156](https://github.com/ponylang/ponyc/pull/3156))
- Add `pointer.offset` to get arbitrary pointer tags via offset ([PR #3177](https://github.com/ponylang/ponyc/pull/3177))
- Add DTrace/SystemTap probes for muted & unmuted events ([PR #3196](https://github.com/ponylang/ponyc/pull/3196))
- Add method to AsioEvent to see if an event is a oneshot ([PR #3198](https://github.com/ponylang/ponyc/pull/3198))

### Changed

- Do not permit leading zeros in JSON numbers ([PR #3167](https://github.com/ponylang/ponyc/pull/3167))
- Make TCPConnection yield on writes to not hog cpu ([PR #3176](https://github.com/ponylang/ponyc/pull/3176))
- Change how TCP connection reads data to improve performance ([PR #3178](https://github.com/ponylang/ponyc/pull/3178))
- Allow use of runtime TCP connect without ASIO one shot ([PR #3171](https://github.com/ponylang/ponyc/pull/3171))
- Simplify buffering in TCPConnection ([PR #3185](https://github.com/ponylang/ponyc/pull/3185))
- Allow for fine-grained control of yielding CPU in TCPConnection ([PR #3197](https://github.com/ponylang/ponyc/pull/3187))
- Make TCPConnection.expect partial ([PR #3197](https://github.com/ponylang/ponyc/pull/3197))

## [0.28.1] - 2019-06-01

### Fixed

- Don't turn off Epoll OneShot when resubscribing to events  ([PR #3136](https://github.com/ponylang/ponyc/pull/3136))
- Add unchop to array and unchop, repeat_str and mul to string ([PR #3155](https://github.com/ponylang/ponyc/pull/3155))
- Fix cycle detector issue with checking for blocked actors ([PR #3154](https://github.com/ponylang/ponyc/pull/3154))
- Wake up suspended scheduler threads if there is work for another one ([PR #3153](https://github.com/ponylang/ponyc/pull/3153))

### Added

- Add Docker image based on Alpine ([PR #3138](https://github.com/ponylang/ponyc/pull/3135))
- Added `XorOshiro128StarStar` and `SplitMix64` PRNGs. ([PR #3135](https://github.com/ponylang/ponyc/pull/3135))
- Added function `Random.int_unbiased`. ([PR #3135](https://github.com/ponylang/ponyc/pull/3135))
- Added `from_u64` constructor to 128 bit state PRNGs. ([PR #3135](https://github.com/ponylang/ponyc/pull/3135))
- Add unchop to array and unchop, repeat_str and mul to string ([PR #3155](https://github.com/ponylang/ponyc/pull/3155))

### Changed

- Updated `XorOshiro128Plus` parameters. This PRNG will now produce different values for the same seed as before. ([PR #3135](https://github.com/ponylang/ponyc/pull/3135))
- Use fixed-point inversion for `Random.int` if the platform allows. ([PR #3135](https://github.com/ponylang/ponyc/pull/3135))

## [0.28.0] - 2019-03-22

### Fixed

- Ensure methods reached through a union are detected as reachable. ([PR #3102](https://github.com/ponylang/ponyc/pull/3102))
- Fixed issue with ponyc not being able to find Visual Studio Build Tools in non-standard locations. ([PR #3086](https://github.com/ponylang/ponyc/pull/3086))
- Update TCPConnection docs around local and remote addresses ([PR #3050](https://github.com/ponylang/ponyc/pull/3050))

### Changed

- Stop Supporting Ubuntu Artful ([PR #3100](https://github.com/ponylang/ponyc/pull/3100))
- Upgrade Docker image to use LLVM 7.0.1 ([PR #3079](https://github.com/ponylang/ponyc/pull/3079))
- Stop trying to be clever when finding user's LLVM installation ([PR #3077](https://github.com/ponylang/ponyc/pull/3077))
- Stop supporting Ubuntu Trusty ([PR #3076](https://github.com/ponylang/ponyc/pull/3076))
- Stop supporting Debian Jessie ([PR #3078](https://github.com/ponylang/ponyc/pull/3078))

## [0.27.0] - 2019-03-01

### Fixed

- Fix default arguments not being displayed correctly in generated docs ([PR #3018](https://github.com/ponylang/ponyc/pull/3018))
- Fix linker library version error on MacOS when installed via Homebrew ([PR #2998](https://github.com/ponylang/ponyc/pull/2998))
- Correctly specify linkage on Windows for runtime library backpressure functions. ([PR #2989](https://github.com/ponylang/ponyc/pull/2989))
- Reject Pointer and MaybePointer from being embedded ([PR #3006](https://github.com/ponylang/ponyc/pull/3006))

### Added

- Add FreeBSD 12 with LLVM 7 as supported platform ([PR #3039](https://github.com/ponylang/ponyc/pull/3039))
- Windows implementation of Process and Pipe. ([PR #3019](https://github.com/ponylang/ponyc/pull/3019))
- LLVM 7.0.1 compatibility ([PR #2976](https://github.com/ponylang/ponyc/pull/2976))
- [RFC 61] Add Modulo Operator (and floored division) ([PR #2997](https://github.com/ponylang/ponyc/pull/2997))
- Update Windows build system to handle the latest Visual Studio 2017 version ([PR #2992](https://github.com/ponylang/ponyc/pull/2992))

### Changed

- make String.f32() and String.f64() partial ([PR #3043](https://github.com/ponylang/ponyc/pull/3043))
- Change `--ponyminthreads` default to `0` ([PR #3020](https://github.com/ponylang/ponyc/pull/3020))
- Change default to disable pinning of scheduler threads to CPU cores ([PR #3024](https://github.com/ponylang/ponyc/pull/3024))
- Fix linker library version error on MacOS when installed via Homebrew ([PR #2998](https://github.com/ponylang/ponyc/pull/2998))

## [0.26.0] - 2019-01-25

### Fixed

- Fix signed/unsigned conversion bug in siphash24 implementation ([PR #2979](https://github.com/ponylang/ponyc/pull/2979))
- Fixes for telemetry.stp script ([PR #2983](https://github.com/ponylang/ponyc/pull/2983))
- libponyc: resolve relative paths in use "path:..." statements ([PR #2964](https://github.com/ponylang/ponyc/pull/2964))
- Fix silent crash with incorrect time formats in Windows ([PR #2971](https://github.com/ponylang/ponyc/pull/2971))
- Add whitespace between multiple args on cli help output ([PR #2942](https://github.com/ponylang/ponyc/pull/2942))
- Fix unsafe cases in capability subtyping implementation ([PR #2660](https://github.com/ponylang/ponyc/pull/2660))
- Fix race condition with socket close event from peer ([PR #2923](https://github.com/ponylang/ponyc/pull/2923))
- Link to libexecinfo on BSD always, not just in a debug compiler ([PR #2916](https://github.com/ponylang/ponyc/pull/2916))
- Install libdtrace_probes.a into the right directory on FreeBSD ([PR #2919](https://github.com/ponylang/ponyc/pull/2919))
- Fix scheduler thread wakeup bug ([PR #2926](https://github.com/ponylang/ponyc/pull/2926))
- Correct README.md link to AppImage packaging location ([PR #2927](https://github.com/ponylang/ponyc/pull/2927))
- Fix unsoundness when replacing `this` viewpoint in method calls. ([PR #2503](https://github.com/ponylang/ponyc/pull/2503))

### Added

- [RFC 60] Add binary heaps ([PR #2950](https://github.com/ponylang/ponyc/pull/2950))
- Make _SignedInteger and _UnsignedInteger public ([PR #2939](https://github.com/ponylang/ponyc/pull/2939))

### Changed

- Fix silent crash with incorrect time formats in Windows ([PR #2971](https://github.com/ponylang/ponyc/pull/2971))
- Add before/after iteration hooks to ponybench ([PR #2898](https://github.com/ponylang/ponyc/pull/2898))

## [0.25.0] - 2018-10-13

### Fixed

- Fix invalid allocation bug in runtime that allows for segfaults ([PR #2896](https://github.com/ponylang/ponyc/pull/2896))
- `buffered` performance improvements and bug fix ([PR #2890](https://github.com/ponylang/ponyc/pull/2890))
- Fix hash collision handling in persistent map ([PR #2894](https://github.com/ponylang/ponyc/pull/2894))
- Correctly handle modifier keys for TTY stdin on Windows ([PR #2892](https://github.com/ponylang/ponyc/pull/2892))
- Fix installation of libponyrt.bc on Linux ([PR #2893](https://github.com/ponylang/ponyc/pull/2893))
- Fix compilation warning on windows. ([PR #2877](https://github.com/ponylang/ponyc/pull/2877))
- Fix building on Windows in a directory with spaces in its name ([PR #2879](https://github.com/ponylang/ponyc/pull/2879))
- Fix losing data when reading from STDIN. ([PR #2872](https://github.com/ponylang/ponyc/pull/2872))
- Fix validation of provides lists of object literals. ([PR #2860](https://github.com/ponylang/ponyc/pull/2860))
- Fix `files/Path.clean()` not correctly handling multiple `..` ([PR #2862](https://github.com/ponylang/ponyc/pull/2862))
- Fix skipped try-then clauses on return, break and continue statements ([PR #2853](https://github.com/ponylang/ponyc/pull/2853))
- Fix performance and memory consumption issues with `files.FileLines` ([PR #2707](https://github.com/ponylang/ponyc/pull/2707))
- Fix ASIO one shot lost notifications problem ([PR #2897](https://github.com/ponylang/ponyc/pull/2897))

### Added

- Add ability to get iso array from an iso string. ([PR #2889](https://github.com/ponylang/ponyc/pull/2889))
- Add modc method to integer types ([PR #2883](https://github.com/ponylang/ponyc/pull/2883))
- Add divc method to integer types ([PR #2882](https://github.com/ponylang/ponyc/pull/2882))
- [RFC 58] Add partial arithmetic for integer types ([PR #2865](https://github.com/ponylang/ponyc/pull/2865))
- Added OpenBSD support. ([PR #2823](https://github.com/ponylang/ponyc/pull/2823))
- Added `set_up` method to `ponytest.UnitTest` as equivalent to existing `tear_down` method ([PR #2707](https://github.com/ponylang/ponyc/pull/2707))
- Added `keep_line_breaks` argument to function `buffered.Reader.line` ([PR #2707](https://github.com/ponylang/ponyc/pull/2707))

### Changed

- Rename mod and related operators to rem ([PR #2888](https://github.com/ponylang/ponyc/pull/2888))
- Improved CHAMP map ([PR #2894](https://github.com/ponylang/ponyc/pull/2894))
- Use RLIMIT_STACK's current limit for Pthreads stack size if it is sane ([PR #2852](https://github.com/ponylang/ponyc/pull/2852))
- Remove `File.line` method in favor of using `FileLines` ([PR #2707](https://github.com/ponylang/ponyc/pull/2707))

## [0.24.4] - 2018-07-29

### Fixed

- Fix array and string trim_in_place allocation bounds check ([PR #2840](https://github.com/ponylang/ponyc/pull/2840))

## [0.24.3] - 2018-07-24

### Added

- Nothing - this was purely to fix a problem in the previously release

## [0.24.2] - 2018-07-24

### Fixed

- Fix `make arch=XXXXX` command to be able to correctly find libponyrt.

## [0.24.1] - 2018-07-22

### Fixed

- Add libexecinfo to debug builds for BSD ([PR #2826](https://github.com/ponylang/ponyc/pull/2826))

### Added

- Add SSL APLN support ([PR #2816](https://github.com/ponylang/ponyc/pull/2816))
- Make dynamic scheduler scaling more robust and configurable ([PR #2801](https://github.com/ponylang/ponyc/pull/2801))

## [0.24.0] - 2018-06-29

### Fixed

- Always use "binary" mode when opening files on Windows. ([PR #2811](https://github.com/ponylang/ponyc/pull/2811))
- Do not set File._errno when reading less than requested bytes. ([PR #2785](https://github.com/ponylang/ponyc/pull/2785))
- Correctly allocate memory for linker arguments ([PR #2797](https://github.com/ponylang/ponyc/pull/2797))
- Fix build on DragonFly BSD ([PR #2794](https://github.com/ponylang/ponyc/pull/2794))
- Fix some edge cases in code generation for loops that jump away. ([PR #2791](https://github.com/ponylang/ponyc/pull/2791))
- Fix repeat loop symbol tracking to allow more valid cases. ([PR #2786](https://github.com/ponylang/ponyc/pull/2786))
-  Fix incorrect disposable/destroyed epoll resubscribe handling ([PR #2781](https://github.com/ponylang/ponyc/pull/2781))
- Fix GC-safety issue with writev pointers in File. ([PR #2775](https://github.com/ponylang/ponyc/pull/2775))
- Disable neon for armhf if not supported by C/C++ compiler ([PR #2672](https://github.com/ponylang/ponyc/pull/2672))

### Changed

- Run cycle detector every N ms based on timer ([PR #2709](https://github.com/ponylang/ponyc/pull/2709))
- [RFC 55] Remove package net/http from stdlib ([PR #2795](https://github.com/ponylang/ponyc/pull/2795))
- Change refcap of JsonDoc to ref for better usability ([PR #2747](https://github.com/ponylang/ponyc/pull/2747))
- Change NetAddress class to hide its fields behind functions, fixing cross-platform compatibility. ([PR #2734](https://github.com/ponylang/ponyc/pull/2734))

## [0.23.0] - 2018-06-10

### Fixed

- Fix File.writev and File.flush in cases where the IO vector exceeds IOV_MAX. ([PR #2771](https://github.com/ponylang/ponyc/pull/2771))
- Fix incorrect tuple handling ([PR #2763](https://github.com/ponylang/ponyc/pull/2763))
- Fix Promise bug where join() element's reject doesn't reject the entire join ([PR #2770](https://github.com/ponylang/ponyc/pull/2770))

### Added

- Add Integer.bit_reverse, exposing the llvm.bitreverse intrinsic. ([PR #2710](https://github.com/ponylang/ponyc/pull/2710))

### Changed

- RFC 56: Make buffered.reader.line return String iso^ ([PR #2756](https://github.com/ponylang/ponyc/pull/2756))

## [0.22.6] - 2018-06-07

### Fixed

- Fix compiler segfault caused by dead code removal of tupled variables ([PR #2757](https://github.com/ponylang/ponyc/pull/2757))
- Fix `collections/persistent/Lists.from()` to return elements in the correct order ([PR #2754](https://github.com/ponylang/ponyc/pull/2754))
- Fix performance related to dynamic scheduler scaling ([PR #2751](https://github.com/ponylang/ponyc/pull/2751))
- Fix incorrect disposable/destroyed epoll resubscribe handling ([PR #2744](https://github.com/ponylang/ponyc/pull/2744))
- Fix performance regression in serialization performance ([PR #2752](https://github.com/ponylang/ponyc/pull/2752))

## [0.22.5] - 2018-06-05

### Fixed

- Fix memory overflow when allocating CPUs when numa is enabled ([PR #2745](https://github.com/ponylang/ponyc/pull/2745))

## [0.22.4] - 2018-06-04

### Fixed

- Fix compiler crash related to union types with duplicate members. ([PR #2738](https://github.com/ponylang/ponyc/pull/2738))
- Fix CommandSpec without args and help ([PR #2721](https://github.com/ponylang/ponyc/pull/2721))

## [0.22.3] - 2018-05-31

### Fixed

- Only enable mcx16 for gcc for x86_64 targets ([PR #2725](https://github.com/ponylang/ponyc/pull/2725))
- Fix String.concat ignoring the len parameter ([PR #2723](https://github.com/ponylang/ponyc/pull/2723))

## [0.22.2] - 2018-05-26

### Fixed

- Relax ProcessMonitor checks for execute bits ([PR #2717](https://github.com/ponylang/ponyc/pull/2717))

## [0.22.1] - 2018-05-25

### Fixed

- Broken docker image creation

## [0.22.0] - 2018-05-24

### Fixed

- Incorrect rstrip handling of multibyte characters ([PR #2706](https://github.com/ponylang/ponyc/pull/2706))
- Fix File.flush return value for case of zero bytes to flush. ([PR #2704](https://github.com/ponylang/ponyc/pull/2704))
- Enable virtual terminal color output on Windows. ([PR #2702](https://github.com/ponylang/ponyc/pull/2702))
- Compute File.writeable based on FileWrite instead of FileRead. ([PR #2698](https://github.com/ponylang/ponyc/pull/2698))
- Do not use llvm.smul.with.overflow.i64 anymore ([PR #2693](https://github.com/ponylang/ponyc/pull/2693))
- Change directory.open_file() to use readonly open ([PR #2697](https://github.com/ponylang/ponyc/pull/2697))
- Avoid flattening arrow to type param, fixing typechecking when reified with another type param. ([PR #2692](https://github.com/ponylang/ponyc/pull/2692))
- Fix File.valid() and clarify File behavior in error cases ([PR #2656](https://github.com/ponylang/ponyc/pull/2656))
- Fix tuple pattern matching issue where only some elements would violate caps. ([PR #2658](https://github.com/ponylang/ponyc/pull/2658))
- Fix scheduler suspend edge case assertion ([PR #2641](https://github.com/ponylang/ponyc/pull/2641))
- Fix code generation failure with tuples in recover blocks ([PR #2642](https://github.com/ponylang/ponyc/pull/2642))
- Do not mute sender if sender is under pressure ([PR #2644](https://github.com/ponylang/ponyc/pull/2644))
- Fix FilePath.walk performance issues (issue #2158). ([PR #2634](https://github.com/ponylang/ponyc/pull/2634))
- Fix backpressure-related TCPConnection busy-loop bug (issue #2620). ([PR #2627](https://github.com/ponylang/ponyc/pull/2627))
- Fix off-by-one error in String.cstring ([PR #2616](https://github.com/ponylang/ponyc/pull/2616))
- Fix compiler crash related to array inference. ([PR #2603](https://github.com/ponylang/ponyc/pull/2603))
- Ensure ASIO thread cannot be stopped prematurely ([PR #2612](https://github.com/ponylang/ponyc/pull/2612))
- Handle EAGAIN errors on socket operations ([PR #2611](https://github.com/ponylang/ponyc/pull/2611))
- Properly report type parameter capability errors ([PR #2598](https://github.com/ponylang/ponyc/pull/2598))
- Update the Windows build to support the latest Visual C++ Build Tools ([PR #2594](https://github.com/ponylang/ponyc/pull/2594))
- Fix tracing of boxed tuples through interfaces ([PR #2593](https://github.com/ponylang/ponyc/pull/2593))
- Partially mitigate LLVM's infinite loops bug ([PR #2592](https://github.com/ponylang/ponyc/pull/2592))
- Fix extracting docstring from constructors ([PR #2586](https://github.com/ponylang/ponyc/pull/2586))
- Always unsubscribe process_monitor fds before closing them (#2529) ([PR #2574](https://github.com/ponylang/ponyc/pull/2574))
- Fix two race conditions where an ASIO wakeup notifications can be lost ([PR #2561](https://github.com/ponylang/ponyc/pull/2561))
- Fix compiler crash in alternate name suggestion logic (issue #2508). ([PR #2552](https://github.com/ponylang/ponyc/pull/2552))
- Make the File.open constructor use read-only mode (Issue #2567) ([PR #2568](https://github.com/ponylang/ponyc/pull/2568))
- Correctly typecheck FFI arguments with regard to aliasing ([PR #2550](https://github.com/ponylang/ponyc/pull/2550))
- Allow tuples to match empty interfaces ([PR #2532](https://github.com/ponylang/ponyc/pull/2532))
- Properly report default argument inference errors ([PR #2504](https://github.com/ponylang/ponyc/pull/2504))
- Do not catch foreign exceptions in Pony try blocks. This still doesn't work on Windows, foreign code doesn't catch foreign exceptions if they've traversed a Pony frame. ([PR #2466](https://github.com/ponylang/ponyc/pull/2466))
- Fix LLVM IR verification with DoNotOptimise ([PR #2506](https://github.com/ponylang/ponyc/pull/2506))

### Added

- Add addc, subc and mulc functions to I128 and U128 ([PR #2645](https://github.com/ponylang/ponyc/pull/2645))
- Add mechanism to register (noisy) signal handler ([PR #2631](https://github.com/ponylang/ponyc/pull/2631))
- Added `nosupertype` annotation for subtyping exclusion (RFC 54). ([PR #2678](https://github.com/ponylang/ponyc/pull/2678))
- `SourceLoc.type_name` method ([PR #2643](https://github.com/ponylang/ponyc/pull/2643))
- Update with experimental support for LLVM 6.0.0 ([PR #2595](https://github.com/ponylang/ponyc/pull/2595))
- Improve pattern matching error reporting ([PR #2628](https://github.com/ponylang/ponyc/pull/2628))
- Allow recovering at most one element of a tuple to mutable capability. ([PR #2585](https://github.com/ponylang/ponyc/pull/2585))
- Add full'ish support for network socket get & set options ([PR #2513](https://github.com/ponylang/ponyc/pull/2513))
- Add basic compiler plugins ([PR #2566](https://github.com/ponylang/ponyc/pull/2566))
- [RFC 50] add member docstrings ([PR #2543](https://github.com/ponylang/ponyc/pull/2543))
- Allow user defined help string at CommandSpec.add_help() ([PR #2535](https://github.com/ponylang/ponyc/pull/2535))
- Embed source code into generated documentation. ([PR #2439](https://github.com/ponylang/ponyc/pull/2439))
- Compile error when comparing sugared constructors with 'is' or 'isnt' (#2024) ([PR #2494](https://github.com/ponylang/ponyc/pull/2494))
- Support OpenSSL 1.1.0 ([PR #2415](https://github.com/ponylang/ponyc/pull/2415))
- Add U64 type to `cli` package. ([PR #2488](https://github.com/ponylang/ponyc/pull/2488))
- Allow customisation of `Env.input` and `Env.exitcode` in artificial environments
- `hash64()` function and related helper types in `collections` for 64-bit hashes ([PR #2615](https://github.com/ponylang/ponyc/pull/2615))

### Changed

- `SourceLoc.method` renamed as `SourceLoc.method_name` method ([PR #2643](https://github.com/ponylang/ponyc/pull/2643))
- New Ponybench API (RFC 52) ([PR #2578](https://github.com/ponylang/ponyc/pull/2578))
- Forbid impossible pattern matching on generic capabilities ([PR #2499](https://github.com/ponylang/ponyc/pull/2499))
- Remove case functions ([PR #2542](https://github.com/ponylang/ponyc/pull/2542))
- Rename Date to PosixDate ([PR #2436](https://github.com/ponylang/ponyc/pull/2436))
- Fix and re-enable dynamic scheduler scaling ([PR #2483](https://github.com/ponylang/ponyc/pull/2483))
- Expose OutStream rather than StdStream in Env ([PR #2463](https://github.com/ponylang/ponyc/pull/2463))
- Rename StdinNotify to InputNotify
- `digestof` and `hash()` now return USize instead of U64 ([PR #2615](https://github.com/ponylang/ponyc/pull/2615))

## [0.21.3] - 2018-01-14

### Fixed

- Fix pthread condition variable usage for dynamic scheduler scaling ([PR #2472](https://github.com/ponylang/ponyc/pull/2472))
- Fix various memory leaks ([PR #2479](https://github.com/ponylang/ponyc/pull/2479))
- Fix double free in expr_typeref ([PR #2467](https://github.com/ponylang/ponyc/pull/2467))
- Fix some spurious process manager test failures ([PR #2452](https://github.com/ponylang/ponyc/pull/2452))
- Fix ANTLR definition for char escape sequences ([PR #2440](https://github.com/ponylang/ponyc/pull/2440))

### Added

- Add the pony_try function to receive Pony errors in C code ([PR #2457](https://github.com/ponylang/ponyc/pull/2457))

### Changed

- Disable -avx512f on LLVM < 5.0.0 to avoid LLVM bug 30542 ([PR #2475](https://github.com/ponylang/ponyc/pull/2475))
- Deprecate LLVM 3.7.1 and 3.8.1 support ([PR #2461](https://github.com/ponylang/ponyc/pull/2461))

## [0.21.2] - 2017-12-26

### Fixed

- Don't suspend schedulers if terminating and reset steal_attempts on wake ([PR #2447](https://github.com/ponylang/ponyc/pull/2447))

## [0.21.1] - 2017-12-23

### Added

- Dynamic scheduler thread scaling based on workload ([PR #2386](https://github.com/ponylang/ponyc/pull/2386))
- Support setting the binary executable name with `--bin-name`. ([PR #2430](https://github.com/ponylang/ponyc/pull/2430))

## [0.21.0] - 2017-12-17

### Fixed

- Forbid structs with embed fields with finalisers ([PR #2420](https://github.com/ponylang/ponyc/pull/2420))
- Fix codegen ordering of implicit finalisers ([PR #2419](https://github.com/ponylang/ponyc/pull/2419))
- Fix GC tracing of struct fields ([PR #2418](https://github.com/ponylang/ponyc/pull/2418))
- Remove redundant error message for unhandled partial calls that are actually in a try block. ([PR #2411](https://github.com/ponylang/ponyc/pull/2411))
- Fix allocation sizes in runtime benchmarks ([PR #2383](https://github.com/ponylang/ponyc/pull/2383))
- Fail pony_start if ASIO backend wasn't successfully initialized ([PR #2381](https://github.com/ponylang/ponyc/pull/2381))
- Make windows sleep consistent with non-windows sleep ([PR #2382](https://github.com/ponylang/ponyc/pull/2382))
- Fix Iter.{skip,take,nth} to check '.has_next()' of their inner iterator ([PR #2377](https://github.com/ponylang/ponyc/pull/2377))
- Restart ASIO if needed while runtime is attempting to terminate. ([PR #2373](https://github.com/ponylang/ponyc/pull/2373))
- fix Range with negative or 0 step and allow backward Ranges (having `min > max`) ([PR #2350](https://github.com/ponylang/ponyc/pull/2350))
- Improve work-stealing "scheduler is blocked" logic ([PR #2355](https://github.com/ponylang/ponyc/pull/2355))
- Make take_while short-circuit ([PR #2358](https://github.com/ponylang/ponyc/pull/2358))
- Fix compilation error with 'use=dtrace' for FreeBSD 11 ([PR #2343](https://github.com/ponylang/ponyc/pull/2343))
- Fix Set.intersect ([PR #2361](https://github.com/ponylang/ponyc/pull/2361))
-  Fixed state handling of HTTP client connections ([PR #2273](https://github.com/ponylang/ponyc/pull/2273))
- Fix incorrect detection of exhaustive matches for structural equality comparisons on some primitives. ([PR #2342](https://github.com/ponylang/ponyc/pull/2342))
- Fix poor randomness properties of first call to `Rand.next()`. ([PR #2321](https://github.com/ponylang/ponyc/pull/2321))
- Fully close unspecified family TCP connections on Windows. ([PR #2325](https://github.com/ponylang/ponyc/pull/2325))
- Make ContentsLogger implement the Logger interface ([PR #2330](https://github.com/ponylang/ponyc/pull/2330))
- Fix alloc bug in String/Array trimming functions ([PR #2336](https://github.com/ponylang/ponyc/pull/2336))
- Fix small chunk finaliser premature re-use bug ([PR #2335](https://github.com/ponylang/ponyc/pull/2335))
- Make Payload.respond() send given parameter, not `this`. ([PR #2324](https://github.com/ponylang/ponyc/pull/2324))
- Garbage collect actors when --ponynoblock is in use ([PR #2307](https://github.com/ponylang/ponyc/pull/2307))
- Fix incorrect kevent structure size ([PR #2312](https://github.com/ponylang/ponyc/pull/2312))
- Fix possible repetition in Iter.flat_map ([PR #2304](https://github.com/ponylang/ponyc/pull/2304))

### Added

- Add DTrace probes for all message push and pop operations ([PR #2295](https://github.com/ponylang/ponyc/pull/2295))
- Experimental support for LLVM 4.0.1 (#1592) and 5.0.0. ([PR #2303](https://github.com/ponylang/ponyc/pull/2303))
- Add pony stable to docker image ([PR #2364](https://github.com/ponylang/ponyc/pull/2364))
- Enable CodeView debug information with MSVC on Windows ([PR #2334](https://github.com/ponylang/ponyc/pull/2334))
- Generalized runtime backpressure. ([PR #2264](https://github.com/ponylang/ponyc/pull/2264))
- A microbenchmark for measuring message passing rates in the Pony runtime. ([PR #2347](https://github.com/ponylang/ponyc/pull/2347))
- Add `chop` function for chopping `iso` Strings and Arrays ([PR #2337](https://github.com/ponylang/ponyc/pull/2337))
- Add --ponyversion option to compiled binary ([PR #2318](https://github.com/ponylang/ponyc/pull/2318))
- Implement RFC 47 (Serialise signature) ([PR #2272](https://github.com/ponylang/ponyc/pull/2272))

### Changed

- Remove unused FormatSettings interface and related types. ([PR #2397](https://github.com/ponylang/ponyc/pull/2397))
- Error on unreachable cases in match expressions and illegal as expressions. ([PR #2289](https://github.com/ponylang/ponyc/pull/2289))

## [0.20.0] - 2017-10-17

### Fixed

- Forbid single '_' in case expr and case methods ([PR #2269](https://github.com/ponylang/ponyc/pull/2269))

### Changed

- Turn off LTO by default on OSX ([PR #2284](https://github.com/ponylang/ponyc/pull/2284))
- Replace memory-leak-inciting `Writer.reserve` with more intuitive `Writer.reserve_current` method. ([PR #2260](https://github.com/ponylang/ponyc/pull/2260))

## [0.19.3] - 2017-10-09

### Fixed

- Don't verify partial calls for method bodies inherited from a trait. ([PR #2261](https://github.com/ponylang/ponyc/pull/2261))
- Fix broken method and type headings in generated documentation ([PR #2262](https://github.com/ponylang/ponyc/pull/2262))
- Fix array inference from ReadSeq interface with tuple element type. ([PR #2259](https://github.com/ponylang/ponyc/pull/2259))
- Fix compiler crash related to inferred lambda argument in apply sugar. ([PR #2258](https://github.com/ponylang/ponyc/pull/2258))
- Fix excess work stealing under low loads ([PR #2254](https://github.com/ponylang/ponyc/pull/2254))
- Fix compiler crash on case methods with `_` as a method parameter. ([PR #2252](https://github.com/ponylang/ponyc/pull/2252))
- Fix implicit fallthrough in array inference. ([PR #2251](https://github.com/ponylang/ponyc/pull/2251))
- Fix small chunk finaliser bug. ([PR #2257](https://github.com/ponylang/ponyc/pull/2257))

## [0.19.2] - 2017-09-24

### Fixed

- Fix codegen failure on field access ([PR #2244](https://github.com/ponylang/ponyc/pull/2244))
- Make Windows link step use the command-line `--linker` value if present. ([PR #2231](https://github.com/ponylang/ponyc/pull/2231))
- Fix empty string serialisation ([PR #2247](https://github.com/ponylang/ponyc/pull/2247))

### Added

- Added Array.swap_elements, Random.shuffle and extends Random with methods for generating all Integer types (RFC 46). ([PR #2128](https://github.com/ponylang/ponyc/pull/2128))

## [0.19.1] - 2017-09-14

### Fixed

- Fix broken "make" command ([PR #2220](https://github.com/ponylang/ponyc/pull/2220))
- Fix inconsistencies in multi-line triple-quoted strings ([PR #2221](https://github.com/ponylang/ponyc/pull/2221))
- Fix undersized string buffer for library link command in Windows. ([PR #2223](https://github.com/ponylang/ponyc/pull/2223))
- Fix Iter.take to handle infinite iterator ([PR #2212](https://github.com/ponylang/ponyc/pull/2212))
- Fix handling of empty and multi-byte character literals ([PR #2214](https://github.com/ponylang/ponyc/pull/2214))

### Added

- Inference of lambda type and array element type from an antecedent (RFC 45). ([PR #2168](https://github.com/ponylang/ponyc/pull/2168))

## [0.19.0] - 2017-09-02

### Fixed

- Fix codegen failures on incompatible FFI declarations ([PR #2205](https://github.com/ponylang/ponyc/pull/2205))
- Disallow named arguments for methods of a type union where parameter names differ ([PR #2194](https://github.com/ponylang/ponyc/pull/2194))
- Fix compiler crash on illegal read from '_' ([PR #2201](https://github.com/ponylang/ponyc/pull/2201))
- Fix signals on Sierra ([PR #2195](https://github.com/ponylang/ponyc/pull/2195))
- Fix race condition in kqueue event system implementation ([PR #2193](https://github.com/ponylang/ponyc/pull/2193))

### Added

- `pony_chain` runtime function

### Changed

- The `pony_sendv` and `pony_sendv_single` runtime functions now take a message chain instead of a single message
- Improve the itertools API (RFC 49) ([PR #2190](https://github.com/ponylang/ponyc/pull/2190))
- Forbid struct finalisers ([PR #2202](https://github.com/ponylang/ponyc/pull/2202))

## [0.18.1] - 2017-08-25

### Fixed

- Don't print capabilities for type params when generating docs ([PR #2184](https://github.com/ponylang/ponyc/pull/2184))

### Added

- DragonFly BSD 4.8 support ([PR #2183](https://github.com/ponylang/ponyc/pull/2183))
- Process monitor async write buffering ([PR #2186](https://github.com/ponylang/ponyc/pull/2186))

## [0.18.0] - 2017-08-19

### Fixed

- Fix compiler crash on union-of-tuples to tuple conversions ([PR #2176](https://github.com/ponylang/ponyc/pull/2176))
- Fix compiler error on lambda capture of '_' ([PR #2171](https://github.com/ponylang/ponyc/pull/2171))
- Fix read past the end of a buffer in `pool.c`. ([PR #2139](https://github.com/ponylang/ponyc/pull/2139))

### Changed

- Make actor continuations a build time option ([PR #2179](https://github.com/ponylang/ponyc/pull/2179))
- RFC #48 Change String.join to take Iterable ([PR #2159](https://github.com/ponylang/ponyc/pull/2159))
- Change fallback linker to "gcc" from "gcc-6" on Linux. ([PR #2166](https://github.com/ponylang/ponyc/pull/2166))
- Change the signature of `pony_continuation` and `pony_triggergc` to take a `pony_ctx_t*` instead of a `pony_actor_t*`.

## [0.17.0] - 2017-08-05

### Fixed

- Fix cursor location for displaying compiler errors and info. ([PR #2136](https://github.com/ponylang/ponyc/pull/2136))
- Fix indent detection when lexing docstrings that contain whitespace-only lines. ([PR #2131](https://github.com/ponylang/ponyc/pull/2131))
- Fix compiler crash on typecheck error in FFI call. ([PR #2124](https://github.com/ponylang/ponyc/pull/2124))
- Fix compiler assert on match including structural equality on union type. ([PR #2117](https://github.com/ponylang/ponyc/pull/2117))

### Added

- Support GCC7 on Linux ([PR #2134](https://github.com/ponylang/ponyc/pull/2134))
- Add regex match iterator ([PR #2109](https://github.com/ponylang/ponyc/pull/2109))
- Add more promise methods (RFC 35) ([PR #2084](https://github.com/ponylang/ponyc/pull/2084))
- Add ability to default enable PIC when compiling ponyc ([PR #2113](https://github.com/ponylang/ponyc/pull/2113))

### Changed

- Treat `as` type of array literals as the alias of the element type. ([PR #2126](https://github.com/ponylang/ponyc/pull/2126))
- docgen: ignore test types and add cli flag to only document public types ([PR #2112](https://github.com/ponylang/ponyc/pull/2112))

## [0.16.1] - 2017-07-30

### Fixed

- Fix reachability analysis for intersection types ([PR #2106](https://github.com/ponylang/ponyc/pull/2106))
- Fix compiler assertion failure at code generation ([PR #2099](https://github.com/ponylang/ponyc/pull/2099))
- FreeBSD builds([PR #2107](https://github.com/ponylang/ponyc/pull/2107))

## [0.16.0] - 2017-07-28

### Fixed

- Fix compiler assertion failure on unused reference to `_` ([PR #2091](https://github.com/ponylang/ponyc/pull/2091))
- Destroy all actors on runtime termination ([PR #2058](https://github.com/ponylang/ponyc/pull/2058))
- Fixed compiler segfault on empty triple quote comment. ([PR #2053](https://github.com/ponylang/ponyc/pull/2053))
- Fix compiler crash on exhaustive match where last case is a union subset ([PR #2049](https://github.com/ponylang/ponyc/pull/2049))
- Make pony_os_std_print() write a newline when given empty input. ([PR #2050](https://github.com/ponylang/ponyc/pull/2050))
- Fix boxed tuple identity when type identifiers differ ([PR #2009](https://github.com/ponylang/ponyc/pull/2009))
- Fix crash on interface function pointer generation ([PR #2025](https://github.com/ponylang/ponyc/pull/2025))

### Added

- Alpine Linux compatibility for pony ([PR #1844](https://github.com/ponylang/ponyc/pull/1844))
- Add cli package implementing the CLI syntax ([RFC #38](https://github.com/ponylang/rfcs/blob/main/text/0038-cli-format.md))
   - Initial ([PR #1897](https://github.com/ponylang/ponyc/pull/1897)) implemented the full RFC and contained:
      - Enhanced Posix / GNU program argument syntax.
      - Commands and sub-commands.
      - Bool, String, I64 and F64 option / arg types.
      - Help command and syntax errors with formatted output.
   - Update ([PR #2019](https://github.com/ponylang/ponyc/pull/2019)) added:
      - String-seq (ReadSeq[String]) types for repeated string options / args.
      - Command fullname() to make it easier to match on unique command names.
      - Checking that commands are leaves so that partial commands return syntax errors.

### Changed

- Forbid returning and passing tuples to FFI functions ([PR #2012](https://github.com/ponylang/ponyc/pull/2012))
- Deprecate support of Clang 3.3
- Explicit partial calls - a question mark is now required to be at the call site for every call to a partial function.
    - See [RFC 39](https://github.com/ponylang/rfcs/blob/main/text/0039-explicit-partial-calls.md).
    - Migration scripts for user code, for convenience, are provided here:
        - [Unix](https://gist.github.com/jemc/95969e3e2b58ddb0dede138c737907f5)
        - [Windows](https://gist.github.com/kulibali/cd5caf3a32d510bb86412f3fd4d52d0f)

## [0.15.0] - 2017-07-08

### Fixed

- Fix bug in `as` capture ([PR #1981](https://github.com/ponylang/ponyc/pull/1981))
- Fix assert failure on lambda parameter missing type. ([PR #2011](https://github.com/ponylang/ponyc/pull/2011))
- Fix iftype where the type variable appears as a type parameter. ([PR #2007](https://github.com/ponylang/ponyc/pull/2007))
- Fix support for recursive constraints in iftype conditions. ([PR #1961](https://github.com/ponylang/ponyc/pull/1961))
- Fix segfault in Array.trim_in_place ([PR #1999](https://github.com/ponylang/ponyc/pull/1999))
- Fix segfault in String.trim_in_place ([PR #1997](https://github.com/ponylang/ponyc/pull/1997))
- Assertion failure with directly recursive trait ([PR #1989](https://github.com/ponylang/ponyc/pull/1989))
- Add compile error for generic Main ([PR #1970](https://github.com/ponylang/ponyc/pull/1970))
- Prevent duplicate long_tests from being registered ([PR #1962](https://github.com/ponylang/ponyc/pull/1962))
- Assertion failure on identity comparison of tuples with different cardinalities ([PR #1946](https://github.com/ponylang/ponyc/pull/1946))
- Stop default arguments from using the function scope ([PR #1948](https://github.com/ponylang/ponyc/pull/1948))
- Fix compiler crash when a field is used in a default argument. ([PR #1940](https://github.com/ponylang/ponyc/pull/1940))
- Fix compiler crash on non-existent field reference in constructor. ([PR #1941](https://github.com/ponylang/ponyc/pull/1941))
- Fix compiler crash on "_" as argument in a case expression. ([PR #1924](https://github.com/ponylang/ponyc/pull/1924))
- Fix compiler crash on type argument error inside an early return. ([PR #1923](https://github.com/ponylang/ponyc/pull/1923))
- Correctly generate debug information in forwarding methods ([PR #1914](https://github.com/ponylang/ponyc/pull/1914))
- Resolved compiler segfault on optimization pass (issue #1225) ([PR #1910](https://github.com/ponylang/ponyc/pull/1910))
- Fix a bug in finaliser handling ([PR #1908](https://github.com/ponylang/ponyc/pull/1908))
- Fix compiler crash for type errors in call arguments inside a tuple. ([PR #1906](https://github.com/ponylang/ponyc/pull/1906))
- Fix compiler crash involving "dont care" symbol in an if or try block. ([PR #1907](https://github.com/ponylang/ponyc/pull/1907))
- Don't call TCPConnection backpressure notifies incorrectly ([PR #1904](https://github.com/ponylang/ponyc/pull/1904))
- Fix outdated methods in persistent.Map docstring. ([PR #1901](https://github.com/ponylang/ponyc/pull/1901))
- Fix format for number types (issue #1920) ([PR #1927](https://github.com/ponylang/ponyc/pull/1927))

### Added

- Make tuples be subtypes of empty interfaces (like Any). ([PR #1937](https://github.com/ponylang/ponyc/pull/1937))
- Add Persistent Vec (RFC 42) ([PR #1949](https://github.com/ponylang/ponyc/pull/1949))
- Add support for custom serialisation (RFC 21) ([PR #1839](https://github.com/ponylang/ponyc/pull/1839))
- Add persistent set (RFC 42) ([PR #1925](https://github.com/ponylang/ponyc/pull/1925))
- Bare methods and bare lambdas (RFC 34) ([PR #1858](https://github.com/ponylang/ponyc/pull/1858))
- xoroshiro128+ implementation ([PR #1909](https://github.com/ponylang/ponyc/pull/1909))
- Exhaustive match ([RFC #40](https://github.com/ponylang/rfcs/blob/main/text/0040-exhaustive-match.md)) ([PR #1891](https://github.com/ponylang/ponyc/pull/1891))
- Command line options for printing help ([PR #1899](https://github.com/ponylang/ponyc/pull/1899))
- `Nanos.from_wall_clock` function to convert from a wall clock as obtained from `Time.now()` into number of nanoseconds since "the beginning of time". ([PR #1967](https://github.com/ponylang/ponyc/pull/1967))

### Changed

- Change machine word constructors to have no default argument. ([PR #1938](https://github.com/ponylang/ponyc/pull/1938))
- Change iftype to use elseif instead of elseiftype as next keyword. ([PR #1905](https://github.com/ponylang/ponyc/pull/1905))
- Removed misleading `Time.wall_to_nanos`. ([PR #1967](https://github.com/ponylang/ponyc/pull/1967))

## [0.14.0] - 2017-05-06

### Fixed

- Compiler error instead of crash for invalid this-dot reference in a trait. ([PR #1879](https://github.com/ponylang/ponyc/pull/1879))
- Compiler error instead of crash for too few args to constructor in case pattern. ([PR #1880](https://github.com/ponylang/ponyc/pull/1880))
- Pony runtime hashmap bug that resulted in issues [#1483](https://github.com/ponylang/ponyc/issues/1483), [#1781](https://github.com/ponylang/ponyc/issues/1781), and [#1872](https://github.com/ponylang/ponyc/issues/1872). ([PR #1886](https://github.com/ponylang/ponyc/pull/1886))
- Compiler crash when compiling to a library ([Issue #1881](https://github.com/ponylang/ponyc/issues/1881))([PR #1890](https://github.com/ponylang/ponyc/pull/1890))

### Changed

- TCPConnection.connect_failed, UDPNotify.not_listening, TCPListenNotify.not_listening no longer have default implementation. The programmer is now required to implement error handling or consciously choose to ignore. ([PR #1853](https://github.com/ponylang/ponyc/pull/1853)

## [0.13.2] - 2017-04-29

### Fixed

- Don’t consider type arguments inside a constraint as constraints. ([PR #1870](https://github.com/ponylang/ponyc/pull/1870))
- Disable mcx16 on aarch64 ([PR #1856](https://github.com/ponylang/ponyc/pull/1856))
- Fix assert failure on explicit reference to `this` in constructor. (issue #1865) ([PR #1867](https://github.com/ponylang/ponyc/pull/1867))
- Compiler crash when using unconstrained type parameters in an `iftype` condition (issue #1689)

## [0.13.1] - 2017-04-22

### Fixed

- Reify function references used as addressof operands correctly ([PR #1857](https://github.com/ponylang/ponyc/pull/1857))
- Properly account for foreign objects in GC ([PR #1842](https://github.com/ponylang/ponyc/pull/1842))
- Compiler crash when using the `addressof` operator on a function with an incorrect number of type arguments

### Added

- Iftype conditions (RFC 26) ([PR #1855](https://github.com/ponylang/ponyc/pull/1855))

## [0.13.0] - 2017-04-07

### Fixed

- Do not allow capability subtyping when checking constraint subtyping. ([PR #1816](https://github.com/ponylang/ponyc/pull/1816))
- Allow persistent map to use any hash function ([PR #1799](https://github.com/ponylang/ponyc/pull/1799))

### Changed

- Pass # of times called to TCPConnectionNotify.received ([PR #1777](https://github.com/ponylang/ponyc/pull/1777))

## [0.12.3] - 2017-04-01

### Fixed

- Improve Visual Studio and Microsoft C++ Build Tools detection. ([PR #1794](https://github.com/ponylang/ponyc/pull/1794))

## [0.12.2] - 2017-03-30

### Fixed

- Fix extreme CPU use in scheduler on Windows ([PR #1785](https://github.com/ponylang/ponyc/pull/1785))
- Fix broken ponytest "only" filter ([PR #1780](https://github.com/ponylang/ponyc/pull/1780))

## [0.12.1] - 2017-03-29

### Fixed

- Bug in ponytest resulted in all tests being skipped ([PR #1778](https://github.com/ponylang/ponyc/pull/1778))

## [0.12.0] - 2017-03-29

### Fixed

- Don't ignore buffer length when printing ([PR #1768](https://github.com/ponylang/ponyc/pull/1768))
- Ifdef out ANSITerm signal handler for SIGWINCH ([PR #1763](https://github.com/ponylang/ponyc/pull/1763))
- Fix build error on 32 bits systems ([PR #1762](https://github.com/ponylang/ponyc/pull/1762))
- Fix annotation-related compiler assertion failure (issue #1751) ([PR #1757](https://github.com/ponylang/ponyc/pull/1757))
- Improve packaged Linux binary performance ([PR #1755](https://github.com/ponylang/ponyc/pull/1755))
- Fix false positive test failure on 32 bits ([PR #1749](https://github.com/ponylang/ponyc/pull/1749))

### Added

- Support XCode 8.3 and LLVM 3.9 ([PR #1765](https://github.com/ponylang/ponyc/pull/1765))

### Changed

- Arrays as sequences ([PR #1741](https://github.com/ponylang/ponyc/pull/1741))
- Add ability for ponytest to exclude tests based on name ([PR #1717](https://github.com/ponylang/ponyc/pull/1717))
- Renamed ponytest "filter" flag to "only" ([PR #1717](https://github.com/ponylang/ponyc/pull/1717))

## [0.11.4] - 2017-03-23

### Fixed

- Identity comparison of boxed values ([PR #1726](https://github.com/ponylang/ponyc/pull/1726))
- Reify type refs in inherited method bodies ([PR #1722](https://github.com/ponylang/ponyc/pull/1722))
- Fix compilation error on non-x86 systems ([PR #1718](https://github.com/ponylang/ponyc/pull/1718))
- Call finalisers for embedded fields when parent type has no finalizer. ([PR #1629](https://github.com/ponylang/ponyc/pull/1629))
- Fix compiling errors for 32-bit ([PR #1709](https://github.com/ponylang/ponyc/pull/1709))
- Improved persistent map api (RFC 36) ([PR #1705](https://github.com/ponylang/ponyc/pull/1705))
- Fix compiler assert on arrow to typeparam in constraint. ([PR #1701](https://github.com/ponylang/ponyc/pull/1701))
- Segmentation fault on runtime termination.

## [0.11.3] - 2017-03-16

### Fixed

- Build Linux release binaries correctly ([PR #1699](https://github.com/ponylang/ponyc/pull/1699))

## [0.11.2] - 2017-03-16

### Fixed

- Fix illegal instruction errors on older cpus when using packaged Pony ([PR #1686](https://github.com/ponylang/ponyc/pull/1686))
- Correctly pass `arch=` when building docker image ([PR #1681](https://github.com/ponylang/ponyc/pull/1681))

### Changed

- Make buffered.Reader.append accept any ByteSeq. ([PR #1644](https://github.com/ponylang/ponyc/pull/1644))

## [0.11.1] - 2017-03-14

### Fixed

- Fix AVX/AVX2 issues with prebuilt ponyc ([PR #1663](https://github.com/ponylang/ponyc/pull/1663))
- Fix linking with '--as-needed' (sensitive to linking order) ([PR #1654](https://github.com/ponylang/ponyc/pull/1654))
- Fix FreeBSD 11 compilation

## [0.11.0] - 2017-03-11

### Fixed

- Make HTTPSession type tag by default ([PR #1650](https://github.com/ponylang/ponyc/pull/1650))
- Fix type parameters not being visible to a lambda type in a type alias ([PR #1633](https://github.com/ponylang/ponyc/pull/1633))
- Remove the check for union types on match error ([PR #1630](https://github.com/ponylang/ponyc/pull/1630))
- TCPListener: unsubscribe asio before socket close ([PR #1626](https://github.com/ponylang/ponyc/pull/1626))
- Fix buffer overflow in case method docstring ([PR #1615](https://github.com/ponylang/ponyc/pull/1615))
- Fix capability checking for gencap-constrained type parameters. ([PR #1593](https://github.com/ponylang/ponyc/pull/1593))
- Fix error in ANTLR grammar regarding duplicate '-~'. (#1602) ([PR #1604](https://github.com/ponylang/ponyc/pull/1604))
- Escape special characters in ANLTR strings. (#1600) ([PR #1601](https://github.com/ponylang/ponyc/pull/1601))
- Use LLVM to detect CPU features by default if --features aren't specified. ([PR #1580](https://github.com/ponylang/ponyc/pull/1580))
- Always call finalisers for embedded fields ([PR #1586](https://github.com/ponylang/ponyc/pull/1586))
- Check for null terminator in String._append ([PR #1582](https://github.com/ponylang/ponyc/pull/1582))
- Fix TCP Connection data receive race condition ([PR #1578](https://github.com/ponylang/ponyc/pull/1578))
- Fix Linux epoll event resubscribe performance and race condition. ([PR #1564](https://github.com/ponylang/ponyc/pull/1564))
- Correctly resubscribe TCPConnection to ASIO events after throttling ([PR #1558](https://github.com/ponylang/ponyc/pull/1558))
- Performance fix in the runtime actor schedule ([PR #1521](https://github.com/ponylang/ponyc/pull/1521))
- Disallow type parameter names shadowing other types. ([PR #1526](https://github.com/ponylang/ponyc/pull/1526))
- Don't double resubscribe to asio events in TCPConnection ([PR #1509](https://github.com/ponylang/ponyc/pull/1509))
- Improve Map.get_or_else performance ([PR #1482](https://github.com/ponylang/ponyc/pull/1482))
- Back pressure notifications now given when encountered while sending data during `TCPConnection` pending writes
- Improve efficiency of muted TCPConnection on non Windows platforms ([PR #1477](https://github.com/ponylang/ponyc/pull/1477))
- Compiler assertion failure during type checking
- Runtime memory allocator bug
- Compiler crash on tuple sending generation (issue #1546)
- Compiler crash due to incorrect subtype assignment (issue #1474)
- Incorrect code generation when sending certain types of messages (issue #1594)

### Added

- Close over free variables in lambdas and object literals ([PR #1648](https://github.com/ponylang/ponyc/pull/1648))
- Add assert_no_error test condition to PonyTest ([PR #1605](https://github.com/ponylang/ponyc/pull/1605))
- Expose `st_dev` and `st_ino` fields of stat structure ([PR #1589](https://github.com/ponylang/ponyc/pull/1589))
- Packed structures (RFC 32) ([PR #1536](https://github.com/ponylang/ponyc/pull/1536))
- Add `insert_if_absent` method to Map ([PR #1519](https://github.com/ponylang/ponyc/pull/1519))
- Branch prediction annotations (RFC 30) ([PR #1528](https://github.com/ponylang/ponyc/pull/1528))
- Readline interpret C-d on empty line as EOF ([PR #1504](https://github.com/ponylang/ponyc/pull/1504))
- AST annotations (RFC 27) ([PR #1485](https://github.com/ponylang/ponyc/pull/1485))
- Unsafe mathematic and logic operations. Can be faster but can have undefined results for some inputs (issue #993)
- Equality comparison for NetAddress ([PR #1569](https://github.com/ponylang/ponyc/pull/1569))
- Host address comparison for NetAddress ([PR #1569](https://github.com/ponylang/ponyc/pull/1569))

### Changed

- Rename IPAddress to NetAddress ([PR #1559](https://github.com/ponylang/ponyc/pull/1559))
- Remove delegates (RFC 31) ([PR #1534](https://github.com/ponylang/ponyc/pull/1534))
- Upgrade to LLVM 3.9.1 ([PR #1498](https://github.com/ponylang/ponyc/pull/1498))
- Deprecate LLVM 3.6.2 support ([PR #1511](https://github.com/ponylang/ponyc/pull/1511)) ([PR #1502](https://github.com/ponylang/ponyc/pull/1502)) (PR ##1512)
- Ensure TCPConnection is established before writing data to it (issue #1310)
- Always allow writing to `_` (dontcare) ([PR #1499](https://github.com/ponylang/ponyc/pull/1499))
- Methods returning their receiver to allow call chaining have been changed to return either None or some useful value. Generalised method chaining implemented in version 0.9.0 should be used as a replacement. The full list of updated methods follows. No details means that the method now returns None.
  - builtin.Seq
    - reserve
    - clear
    - push
    - unshift
    - append
    - concat
    - truncate
  - builtin.Array
    - reserve
    - compact
    - undefined
    - insert
    - truncate
    - trim_in_place
    - copy_to
    - remove
    - clear
    - push
    - unshift
    - append
    - concat
    - reverse_in_place
  - builtin.String
    - reserve
    - compact
    - recalc
    - truncate
    - trim_in_place
    - delete
    - lower_in_place
    - upper_in_place
    - reverse_in_place
    - push
    - unshift
    - append
    - concat
    - clear
    - insert_in_place
    - insert_byte
    - cut_in_place
    - replace (returns the number of occurrences replaced)
    - strip
    - lstrip
    - rstrip
  - buffered.Reader
    - clear
    - append
    - skip
  - buffered.Writer
    - reserve
    - reserve_chunks
    - number writing functions (e.g. u16_le)
    - write
    - writev
  - capsicum.CapRights0
    - set
    - unset
  - collections.Flag
    - all
    - clear
    - set
    - unset
    - flip
    - union
    - intersect
    - difference
    - remove
  - collections.ListNode
    - prepend (returns whether the node was removed from another List)
    - append (returns whether the node was removed from another List)
    - remove
  - collections.List
    - reserve
    - remove
    - clear
    - prepend_node
    - append_node
    - prepend_list
    - append_list
    - push
    - unshift
    - append
    - concat
    - truncate
  - collections.Map
    - concat
    - compact
    - clear
  - collections.RingBuffer
    - push (returns whether the collection was full)
    - clear
  - collections.Set
    - clear
    - set
    - unset
    - union
    - intersect
    - difference
    - remove
  - files.FileMode
    - exec
    - shared
    - group
    - private
  - files.File
    - seek_start
    - seek_end
    - seek
    - flush
    - sync
  - time.Date
    - normal
  - net.http.Payload
    - update (returns the old value)
  - net.ssl.SSLContext
    - set_cert
    - set_authority
    - set_ciphers
    - set_client_verify
    - set_server_verify
    - set_verify_depth
    - allow_tls_v1
    - allow_tls_v1_1
    - allow_tls_v1_2
- TCP sockets on Linux now use Epoll One Shot
- Non-sendable locals and parameters are now seen as `tag` inside of recover expressions instead of being inaccessible.
- TCP sockets on FreeBSD and MacOSX now use Kqueue one shot
- All arithmetic and logic operations are now fully defined for every input by default (issue #993)
- Removed compiler flag `--ieee-math`
- The `pony_start` runtime function now takes a `language_features` boolean parameter indicating whether the Pony-specific runtime features (e.g. network or serialisation) should be initialised

## [0.10.0] - 2016-12-12

### Fixed

- Don't violate reference capabilities when assigning via a field ([PR #1471](https://github.com/ponylang/ponyc/pull/1471))
- Check errors correctly for method chaining ([PR #1463](https://github.com/ponylang/ponyc/pull/1463))
- Fix compiler handling of type params in stacks (issue #918) ([PR #1452](https://github.com/ponylang/ponyc/pull/1452))
- Fix String.recalc method for cases where no null terminator is found (issue #1446) ([PR #1450](https://github.com/ponylang/ponyc/pull/1450))
- Make space() check if string is null terminated (issue #1426) ([PR #1430](https://github.com/ponylang/ponyc/pull/1430))
- Fix is_null_terminated reading arbitrary memory (issue #1425) ([PR #1429](https://github.com/ponylang/ponyc/pull/1429))
- Set null terminator in String.from_iso_array (issue #1435) ([PR #1436](https://github.com/ponylang/ponyc/pull/1436))

### Added

- Added String.split_by, which uses a string delimiter (issue #1399) ([PR #1434](https://github.com/ponylang/ponyc/pull/1434))
- Extra DTrace/SystemTap probes concerning scheduling.

### Changed

- Behaviour calls return None instead of their receiver (RFC 28) ([PR #1460](https://github.com/ponylang/ponyc/pull/1460))
- Update from_array to prevent a copy (issue #1097) ([PR #1423](https://github.com/ponylang/ponyc/pull/1423))

## [0.9.0] - 2016-11-11

### Fixed

- Stop leaking memory during serialization (issue #1413) ([PR #1414](https://github.com/ponylang/ponyc/pull/1414))
- Fixed compiler segmentation fault when given an invalid target triple. ([PR #1406](https://github.com/ponylang/ponyc/pull/1406))
- Fixed error message when no type arguments are given (issue #1396) ([PR #1397](https://github.com/ponylang/ponyc/pull/1397))
- Fixed compiler assert failure when constructor is called on type intersection (issue #1398) ([PR #1401](https://github.com/ponylang/ponyc/pull/1401))
- Fix compiler assert fail on circular type inference error (issue #1334) ([PR #1339](https://github.com/ponylang/ponyc/pull/1339))
- Performance problem in the scheduler queue when running with many threads (issue #1404)
- Invalid name mangling in generated C headers (issue #1377)

### Added

- Method chaining (RFC #25) ([PR #1411](https://github.com/ponylang/ponyc/pull/1411))
- Iter class methods `all`, `any`, `collect`, `count`, `find`, `last`, `nth`, `run`, `skip`, `skip_while`, `take`, `take_while` (issue #1370)
- Output of `ponyc --version` shows C compiler used to build pony (issue #1245)
- Makefile detects `llvmconfig39` in addition to `llvm-config-3.9` (#1379)
- LLVM 3.9 support

### Changed

- Changed lambda literal syntax to be more concise (issue #1391) ([PR #1400](https://github.com/ponylang/ponyc/pull/1400))

## [0.8.0] - 2016-10-27

### Fixed

- Link the correct version of `libponyrt` when compiling with `--pic` on Linux (issue #1359)

### Added

- Runtime function `pony_send_next`. This function can help optimise some message sending scenarios.
- Floating point `min_normalised`. The function returns the smallest normalised positive number, as `min_value` used to do (issue #1351)

### Changed

- Floating point `min_value` now returns the smallest negative number instead of the smallest normalised positive number (issue #1351)

## [0.7.0] - 2016-10-22

### Fixed

- Concatenate docstrings from case methods (issue #575).

### Added

- TCP read and write backpressure hooks in `TCPConnection` (issue #1311)
- Allow TCP notifiers to cause connections to yield while receiving (issue #1343)

### Changed

- `break` without a value now generates its value from the `else` branch of a loop instead of being an implicit `break None`.
- The `for` loop will now break out of the loop instead of continuing with the following iterations if `Iterator.next` errors.

## [0.6.0] - 2016-10-20

### Fixed

- Compiling ponyrt with Clang versions >= 3.3, < 3.6.
- Restrict mutable tuple recovery to maintain reference capability security (issue #1123)
- Crash in the runtime scheduler queues

### Added

- DTrace and SystemTap support - `use=dtrace`

### Changed

- Replaces `use=telemetry` by DTrace/SystemTap scripts
- `String.cstring()` now always returns a null-terminated string
  (which may result in a copy) while `cpointer()` (also available on
  `Array` objects) returns a pointer to the underlying array as-is
  (issue #1309).

## [0.5.1] - 2016-10-15

### Fixed

- `SSLConnection` ignoring the `sent` notifier method (issue #1268)
- Runtime crash in the runtime scheduler queues (issue #1319)

## [0.5.0] - 2016-10-09

### Fixed

- Memory copy bounds for `String.clone` (issue #1289).
- Security issues in `ProcessMonitor` (issue #1180)
- `SSLConnection` bugs due to missing `sentv` notify method (issue #1282)

### Added

- `Iter` class (issue #1267)
- read_until method on buffered.Reader (RFC 0013)
- `format` package (issue #1285)

### Changed

- `Stringable` interface no longer involves formatting (issue #1285)
- Remove unused error types from ProcessError (issue #1293)
- HTML documentation for expanded union types now adds line breaks to improve readability (issue #1263)

## [0.4.0] - 2016-09-26

### Fixed

- Unexpected message ordering in `ProcessManager` (issue #1265)

### Added

- TCP writev performance improvement by avoiding throwing errors

## [0.3.3] - 2016-09-23

### Fixed

- Incorrect build number generated on Windows when building from non-git directory.
- Stop generating `llvm.invariant.load` for fields of `val` references.
- Embedded fields construction through tuples.

### Added

- Improved error handling for `files` package.
- ProcessMonitor.expect
- ProcessNotify.created
- ProcessNotify.expect

### Changed

- On Linux and FreeBSD, ponyc now uses $CC as the linker if the environment variable is defined.

## [0.3.2] - 2016-09-18

### Fixed

- The `ponyc` version is now consistently set from the VERSION file.
- Stop generating `llvm.invariant.load` intrinsic for "let" references, as these don't necessarily match the semantics of that intrinsic.

### Changed

- The `setversion` and `release` commands have been removed from `Makefile`.
- LTO is again enabled by default on OSX
- make now builds a `release` rather than `debug` build by default

## [0.3.1] - 2016-09-14

### Fixed

- Make sure all scheduler threads are pinned to CPU cores; on Linux/FreeBSD this wasn't the case for the main thread.
- Account for both hyperthreading and NUMA locality when assigning scheduler threads to cores on Linux.
- Stop generating `llvm.invariant.start` intrinsic. It was causing various problems in code generation.
- Buffer overflow triggerable by very long `ponyc` filename (issue #1177).
- Assertion failure in optimisation passes.
- Race condition in scheduler queues on weakly-ordered architectures.
- Issue #1212 by reverting commit e56075d46d7d9e1d8c5e8be7ed0506ad2de98734

### Added

- `--ponypinasio` runtime option for pinning asio thread to a cpu core.
- `--ponynopin` runtime option for not pinning any threads at all.

### Changed

- Path.base now provides option to omit the file extension from the result.
- Map.upsert returns value for upserted key rather than `this`.
- `ponyc --version` now includes llvm version in its output.
- LTO is now disabled by default on OSX.

## [0.3.0] - 2016-08-26

### Fixed

- Check for Main.create before reachability analysis.
- Interface subtyping need not be invariant on type args.
- @fowles: handle regex empty match.
- @praetonus: readline history handling.
- Put unbox constructors on machine words into the vtable.
- @jonas-l: parse URL with omitted password.
- Adjust for ephemerality in cap_single().
- Finalisation always occurs.
- Type checking platform dependent FFI declarations on all platforms.
- Interface subtyping takes receiver capabilities into account.
- Pony-as-library support, particularly pony_register_thread().
- Bug in `HashMap._search`.
- Crashing gc bug caused by "force freeing" objects with finalizers.
- Bug in `String.compare` and `String.compare_sub`.
- Crashing gc bug from using `get` instead of `getorput` in `gc_markactor`.
- Add -rpath to the link command for library paths
- Simplify contains() method on HashMap.
- Lambda captures use the alias of the expression type.
- Trace boxed primitives in union types.
- Use -isystem for LLVM include directory only if it is not in search path.
- Union tuples as return type with machine words (issue #849)
- Incorrect \" handling in character literals
- Incorrect \' handling in string literals
- Compiler crash on type alias of a lambda type.
- Late detection of errors silently emitted in lexer/parser.
- Compiler crash when handling invalid lambda return types
- Memory leak fixed when something sent as iso is then sent as val.
- Compiler crash when handling object literal with uninitialized fields.
- Associate a nice name with function types based on the type of the function.
- Use the nice name for types when generating documentation.
- Compiler crash when generating C library for the exported actors containing functions with variadic return type contatining None
- AST Printing of string literals with quote characters.
- HTTP/1.1 connections are now persistent by default.
- Runtime crash when Main.create is a function instead of a constructor.
- Compiler bug where `as` operator with a lambda literal caused seg fault.
- String.read_int failure on lowest number in the range of signed integers.
- Regex incorrect of len variable when PCRE2_ERROR_NOMEMORY is encountered.
- No longer silently ignores lib paths containing parens on Windows.
- Make linking more dynamic and allow for overriding linker and linker arch.
- Fix issue with creating hex and octal strings if precision was specified.
- Correctly parses Windows 10 SDK versions, and includes new UCRT library when linking with Windows 10 SDK.
- Performance of Array.append and Array.concat (no unnecessary calls to push).
- Performance of Map.upsert and Map.update (don't replace existing keys)
- Segmentation fault from allocating zero-sized struct.
- Segmentation fault from serialising zero-sized array.
- Assertion failure from type-checking in invalid programs.
- Make the offset parameter of String.rfind inclusive of the given index.

### Added

- 32-bit ARM port.
- 32-bit X86 port.
- Embedded fields.
- C-style structs.
- Maybe[A] to encode C-style structs that aren't present.
- OpenFile and CreateFile primitives to return well-typed errors.
- @fowles: String.join
- Array slice, permute, reverse.
- Pooltrack and telemetry runtime builds.
- ifdef expressions for platform dependent code.
- User specified build flags.
- Pure Pony implementation of 128 bit integer maths for 32-bit platforms.
- UDP broadcast for both IPv4 and IPv6.
- Message batching.
- Case functions.
- Timeouts for PonyTest long tests.
- contains() method on HashMap
- contains() method on HashSet
- Support for empty sections in ini parsing.
- --verbose,-V option for compiler informational messages.
- Logger package
- `ArrayValues.rewind()` method.
- Nanos primitive in time package.
- Persistent package, with List and Map
- Custom chunk size for Stdin.
- Itertools package
- TCPConnection.expect
- TCPConnectionNotify.sentv
- HashMap.get_or_else
- ponytest TestHelper.expect_action, complete_action, and fail_action
- ponytest TestHelper.dispose_when_done
- copysign and infinite for floating point numbers
- contains() method on Array
- GC tracing with acquire/release semantics.
- pony_alloc_msg_size runtime function
- `net/WriteBuffer`
- `serialise` package.
- optional parameter in json objects, arrays, and doc to turn on pretty printing
- `DoNotOptimise` primitive
- `compact` method for `Array` and `String`
- `String.push_utf32()` method.
- Allow the use of a LLVM bitcode file for the runtime instead of a static library.
- add iterators to persistent/Map (`keys()`, `values()`, and `pairs()`)
- add notification of terminal resize
- `trim` and `trim_in_place` methods for `Array` and `String`.
- `is_null_terminated` and `null_terminated` methods for `String`.
- `from_iso_array` constructor on `String`.
- `Sort` primitive
- PonyBench package

### Changed

- Interfaces are invariant if they are structurally equivalent.
- Improved type checking with configuration management.
- Improved realloc behaviour after heap_alloc_large.
- Set-based upper bounds for generic constraints.
- Moved the position of a default capability in a type specification.
- Replaced '&' with 'addressof' for taking address in FFI calls.
- @jemc: use half-open ranges for String operations.
- Improved TCPConnection with a dynamically size of buffers
- Drop dynamic LTO detection in the build system.
- Parameterized Array.find and Array.rfind with a comparator.
- `this->` adapted types check match on the upper bounds.
- Renamed `identityof` to `digestof`.
- Moved and renamed `net/Buffer` to `buffered/Reader` and `buffered/Writer`.
- Print compiler error and info messages to stderr instead of stdout.
- `Hashmap.concat` now returns `this` rather than `None`
- Only allow single, internal underscores in numeric literals.
- `ponyc --version` now includes the build type (debug/release) in its output.
- Strings now grow and shrink geometrically.
- Embedded fields can now be constructed from complex expressions containing constructors
- Sendable members of non-sendable objects can be used in recover expressions in certain cases.
- ProcessNotify functions are passed a reference to ProcessMonitor.
- The constructor of `Options` doesn't require an `Env` anymore but just a simple `String` array.

## [0.2.1] - 2015-10-06

### Fixed

- Check shallow marking in heap_ismarked.

## [0.2.0] - 2015-10-05

### Fixed

- Handle internal pointers and recursion.
- Allow recursing through non-pony alloc'd memory in GC.
- Set an LLVM triple with no version stamp to prevent XCode 7 link warnings.
- use "path:" adds link paths only for the current build.
- Handle null characters in Strings and string literals.

### Added

- Platform indicators for LP64, LLP64, ILP32.
- Compile and link with LTO.
- Use Pointer[None] for void* in FFI.
- Root authority capability in Env.
- Fine grained capabilities for files and directories.
- Use Capsicum on FreeBSD.
- Apply can be sugared with type arguments.
- Search pony_packages directories for use commands.
- Buffer peek functions (@jemc)
- collections/Ring
- Promises package

### Changed

- Renamed some builtin types.
- abs() now returns an unsigned integer.
- Improved memory allocation speed.
- Reduced memory pressure.
- Scheduler steals when only the CD is on a scheduler thread queue.
- use commands searches ../pony_packages recursively similar to Node.js
- Readline uses a Promise to handle async responses.

## [0.1.7] - 2015-06-18

### Fixed

- Viewpoint adaptation with a type expression on the left-hand side.

### Added

- Pass Pony function pointers to C FFI.

### Changed

- The pony runtime now uses the same option parser as ponyc. A pony program exits if bad runtime args are provided.
- Output directory now created if it doesn't already exist.
- Improvements to automatic documentation generator.
- Union type for String.compare result.

## [0.1.6] - 2015-06-15

### Fixed

- ANSI stripping on zero length writes to stdout/stderr.
- More OS X 10.8 compatibility.
- SSL multithreading support.
- Nested tuple code generation.
- Only finalise blocked actors when detecting quiescence.
- URL parse error.

### Added

- Automatic documentation generator in the compiler.
- FreeBSD 10.1 support, thanks to Ben Laurie.
- Allow method calls on union types when the signatures are compatible.
- Subtyping of polymorphic methods.
- Primitive `_init` and `_final` for C library initialisation and shutdown.
- collections.Flags
- lambda sugar.

### Changed

- Separated the FFI '&' operator from the identityof operator.
- Operators on Set and Map are now persistent.
- use "file:..." becomes use "package:..."
- Allow "s at the end of triple-quoted strings.
- Allow behaviours and functions to be subtypes of each other.

## [0.1.5] - 2015-05-15

### Fixed

- OS X 10.8 compatibility.

## [0.1.4] - 2015-05-14

### Fixed

- Check whether parameters to behaviours, actor constructors and isolated constructors are sendable after flattening, to allow sendable type parameters to be used as parameters.
- Eliminate spurious "control expression" errors when another compile error has occurred.
- Handle circular package dependencies.
- Fixed ponyc options issue related to named long options with no arguments
- Cycle detector view_t structures are now reference counted.

### Added

- ANSI terminal handling on all platforms, including Windows.
- The lexer now allows underscore characters in numeric literals. This allows long numeric literals to be broken up for human readability.
- "Did you mean?" support when the compiler doesn't recognise a name but something similar is in scope.
- Garbage collection and cycle detection parameters can now be set from the command line.
- Added a FileStream wrapper to the file package.

### Changed

- When using a package without a package identifier (eg. `use "foo"` as opposed to `use f = "foo"`), a `Main` type in the package will not be imported. This allows all packages to include unit tests that are run from their included `Main` actor without causing name conflicts.
- The `for` sugar now wraps the `next()` call in a try expression that does a `continue` if an error is raised.

