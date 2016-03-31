# Change Log

All notable changes to the Pony compiler and standard library will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/) and [Keep a CHANGELOG](http://keepachangelog.com/).

## [unreleased] - unreleased

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
- Support for empty sections in ini parsing.
- --verbose,-V option for compiler informational messages.
- Logger package

### Changed

- Interfaces are invariant if they are structurally equivalent.
- Improved type checking with configuration management.
- Improved realloc behaviour after heap_alloc_large.
- Set-based upper bounds for generic constraints.
- Moved the position of a default capability in a type specification.
- Replaced '&' with 'addressof' for taking address in FFI calls.
- @jemc: use half-open ranges for String operations.

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
