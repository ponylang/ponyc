# Change Log

All notable changes to the Pony compiler and standard library will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/) and [Keep a CHANGELOG](http://keepachangelog.com/).

## [unreleased] - unreleased

### Fixed

- Check errors correctly for method chaining (PR #1463)
- Fix compiler handling of type params in stacks (issue #918) (PR #1452)
- Fix String.recalc method for cases where no null terminator is found (issue #1446) (PR #1450)
- Make space() check if string is null terminated (issue #1426) (PR #1430)
- Fix is_null_terminated reading arbitrary memory (issue #1425) (PR #1429)
- Set null terminator in String.from_iso_array (issue #1435) (PR #1436)

### Added

- Added String.split_by, which uses a string delimiter (issue #1399) (PR #1434)
- Extra DTrace/SystemTap probes concerning scheduling.

### Changed

- Behaviour calls return None instead of their receiver (RFC 28) (PR #1460)
- Update from_array to prevent a copy (issue #1097) (PR #1423)

## [0.9.0] - 2016-11-11

### Fixed

- Stop leaking memory during serialization (issue #1413) (PR #1414)
- Fixed compiler segmentation fault when given an invalid target triple. (PR #1406)
- Fixed error message when no type arguments are given (issue #1396) (PR #1397)
- Fixed compiler assert failure when constructor is called on type intersection (issue #1398) (PR #1401)
- Fix compiler assert fail on circular type inference error (issue #1334) (PR #1339)
- Performance problem in the scheduler queue when running with many threads (issue #1404)
- Invalid name mangling in generated C headers (issue #1377)

### Added

- Method chaining (RFC #25) (PR #1411)
- Iter class methods `all`, `any`, `collect`, `count`, `find`, `last`, `nth`, `run`, `skip`, `skip_while`, `take`, `take_while` (issue #1370)
- Output of `ponyc --version` shows C compiler used to build pony (issue #1245)
- Makefile detects `llvmconfig39` in addition to `llvm-config-3.9` (#1379)
- LLVM 3.9 support

### Changed

- Changed lambda literal syntax to be more concise (issue #1391) (PR #1400)

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
- Readline uses a Promise to handle async reponses.

### Fixed

- Handle internal pointers and recursion.
- Allow recursing through non-pony alloc'd memory in GC.
- Set an LLVM triple with no version stamp to prevent XCode 7 link warnings.
- use "path:" adds link paths only for the current build.
- Handle null characters in Strings and string literals.

## [0.1.7] - 2015-06-18

### Added

- Pass Pony function pointers to C FFI.

### Changed

- The pony runtime now uses the same option parser as ponyc. A pony program exits if bad runtime args are provided.
- Output directory now created if it doesn't already exist.
- Improvements to automatic documentation generator.
- Union type for String.compare result.

### Fixed

- Viewpoint adaptation with a type expression on the left-hand side.

## [0.1.6] - 2015-06-15

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

### Fixed

- ANSI stripping on zero length writes to stdout/stderr.
- More OS X 10.8 compatibility.
- SSL multithreading support.
- Nested tuple code generation.
- Only finalise blocked actors when detecting quiescence.
- URL parse error.

## [0.1.5] - 2015-05-15

### Fixed

- OS X 10.8 compatibility.

## [0.1.4] - 2015-05-14

### Changed

- When using a package without a package identifier (eg. `use "foo"` as opposed to `use f = "foo"`), a `Main` type in the package will not be imported. This allows all packages to include unit tests that are run from their included `Main` actor without causing name conflicts.
- The `for` sugar now wraps the `next()` call in a try expression that does a `continue` if an error is raised.

### Added

- ANSI terminal handling on all platforms, including Windows.
- The lexer now allows underscore characters in numeric literals. This allows long numeric literals to be broken up for human readability.
- "Did you mean?" support when the compiler doesn't recognise a name but something similar is in scope.
- Garbage collection and cycle detection parameters can now be set from the command line.
- Added a FileStream wrapper to the file package.

### Fixed

- Check whether parameters to behaviours, actor constructors and isolated constructors are sendable after flattening, to allow sendable type parameters to be used as parameters.
- Eliminate spurious "control expression" errors when another compile error has occurred.
- Handle circular package dependencies.
- Fixed ponyc options issue related to named long options with no arguments
- Cycle detector view_t structures are now reference counted.
