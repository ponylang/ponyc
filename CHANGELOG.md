# Change Log

All notable changes to the Pony compiler and standard library will be documented in this file. This project adheres to [Semantic Versioning](http://semver.org/) and [Keep a CHANGELOG](http://keepachangelog.com/).

## [unreleased] - unreleased

### Fixed

- Garbage collect actors when --ponynoblock is in use ([PR #2307](https://github.com/ponylang/ponyc/pull/2307))
- Fix incorrect kevent structure size ([PR #2312](https://github.com/ponylang/ponyc/pull/2312))
- Fix possible repetition in Iter.flat_map ([PR #2304](https://github.com/ponylang/ponyc/pull/2304))

### Added

- Add --ponyversion option to compiled binary ([PR #2318](https://github.com/ponylang/ponyc/pull/2318))
- Implement RFC 47 (Serialise signature) ([PR #2272](https://github.com/ponylang/ponyc/pull/2272))

### Changed

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

