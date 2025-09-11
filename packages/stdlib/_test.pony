"""
# Pony Standard Library

This package represents the test suite for the Pony standard library.

For every new package, please add a Main actor and tests to the package in a
file called 'test.pony'. Then add a corresponding use directive and a line to
the main actor constructor of this package to invoke those tests.

All tests can be run by compiling and running packages/stdlib.
"""

// Include ALL standard library packages here, even if they don't have tests.
// That way stdlib can be used to type check the whole standard library,
// generate docs for it, etc.
use "pony_test"
use assert = "assert"
use actor_pinning = "actor_pinning"
use backpressure= "backpressure"
use base64 = "encode/base64"
use buffered = "buffered"
use builtin_test = "builtin_test"
use bureaucracy = "bureaucracy"
use capsicum = "capsicum"
use cli = "cli"
use collections = "collections"
use collections_persistent = "collections/persistent"
use constrained_types = "constrained_types"
use debug = "debug"
use files = "files"
use format = "format"
use ini = "ini"
use itertools = "itertools"
use math = "math"
use net = "net"
use pony_bench = "pony_bench"
use pony_check = "pony_check"
use process = "process"
use promises = "promises"
use random = "random"
use runtime_info = "runtime_info"
use serialise = "serialise"
use signals = "signals"
use strings = "strings"
use term = "term"
use time = "time"


actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    actor_pinning.Main.make().tests(test)
    base64.Main.make().tests(test)
    buffered.Main.make().tests(test)
    builtin_test.Main.make().tests(test)
    bureaucracy.Main.make().tests(test)
    cli.Main.make().tests(test)
    collections.Main.make().tests(test)
    collections_persistent.Main.make().tests(test)
    constrained_types.Main.make().tests(test)
    files.Main.make().tests(test)
    format.Main.make().tests(test)
    ini.Main.make().tests(test)
    itertools.Main.make().tests(test)
    math.Main.make().tests(test)
    net.Main.make().tests(test)
    pony_check.Main.make().tests(test)
    process.Main.make().tests(test)
    promises.Main.make().tests(test)
    random.Main.make().tests(test)
    runtime_info.Main.make().tests(test)
    serialise.Main.make().tests(test)
    strings.Main.make().tests(test)
    time.Main.make().tests(test)

    // Tests below function exclude windows and are listed alphabetically
    ifdef not windows then
      // The signals tests currently abort the process on Windows, so ignore
      // them.
      signals.Main.make().tests(test)
    end

  fun @runtime_override_defaults(rto: RuntimeOptions) =>
     rto.ponynoblock = true
