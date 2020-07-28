# Change Log

All notable changes to the Pony compiler and standard library will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/) and [Keep a CHANGELOG](http://keepachangelog.com/).

## [unreleased] - unreleased

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
- Add cli package implementing the CLI syntax ([RFC #38](https://github.com/ponylang/rfcs/blob/master/text/0038-cli-format.md))
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
    - See [RFC 39](https://github.com/ponylang/rfcs/blob/master/text/0039-explicit-partial-calls.md).
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
- Exhaustive match ([RFC #40](https://github.com/ponylang/rfcs/blob/master/text/0040-exhaustive-match.md)) ([PR #1891](https://github.com/ponylang/ponyc/pull/1891))
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

