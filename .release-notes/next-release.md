## Enable building of libponyc-standalone on MacOS

We now ship a "standalone" version of libponyc for MacOS. libponyc-standalone allows applications to use Pony compiler functionality as a library. The standalone version contains "all dependencies" needed in a single library. On MacOS, sadly "all dependencies" means "all that can be statically linked", so unlike Linux, dynamically linking to C++ standard library is required on MacOS.

An example pony program linking against it would need to look like this:

```pony
use "lib:ponyc-standalone" if posix or osx
use "lib:c++" if osx

actor Main
  new create(env: Env) =>
    None
```

## Update DTrace probes interface documentation

Our [DTrace]() probes needed some care. Over time, the documented interface and the interface itself had drifted. We've updated all probe interface definitions to match the current implementations.

## Improve usability of some DTrace probes

A number of runtime [DTrace]() probes were lacking information required to make them useful. The probes for:

- GC Collection Starting
- GC Collection Ending
- GC Object Receive Starting
- GC Object Receive Ending
- GC Object Send Starting
- GC Object Send Ending
- Heap Allocation

All lacked information about the actor in question. Instead they only contained information about the scheduler that was active at the time the operation took place. Without information about the actor doing a collection, allocating memory, etc the probes had little value.

## Remove ambiguity from "not safe to write" compiler error message

Previously, when displaying the "not safe to write" error message, the information provided was incomplete in a way that lead to ambiguity. A "not safe to write" error involves two components, the left side type being assigned to and the right side type being assigned to the left side. We were only displaying the right side, not the left side type, thus making it somewhat difficult to know exactly what the error was.
As an example, when trying to write to a field in an iso class instance, we get:

```
Error:
/Users/jairocaro-accinoviciana/issues-pony-4290/demo.pony:10:11: not safe to write right side to left side
    x.foo = foo
          ^
    Info:
    /Users/jairocaro-accinoviciana/issues-pony-4290/demo.pony:9:14: right side type: Foo ref
        let foo: Foo ref = Foo
                 ^
```

The updated error message is now in the format:

```
Error:
/Users/jairocaro-accinoviciana/issues-pony-4290/demo.pony:10:11: not safe to write right side to left side
    x.foo = foo
          ^
    Info:
    /Users/jairocaro-accinoviciana/issues-pony-4290/demo.pony:9:14: right side type: Foo ref
        let foo: Foo ref = Foo
                 ^
    /Users/jairocaro-accinoviciana/issues-pony-4290/demo.pony:10:5: left side type: X iso
        x.foo = foo
        ^
```

## Stop building "x86-64-unknown-linux-gnu" packages

Previously we built a "generic gnu" ponyc package and made it available via Cloudsmith and ponyup. Unfortunately, there is no such thing as a "generic gnu" ponyc build that is universally usable. The ponyc package only worked if the library versions on your Glibc based Linux distribution matched those of our build environment.

We've stoped building the "generic gnu" aka "x86-64-unknown-linux-gnu" packages of ponyc as they had limited utility and a couple of different ways of creating a very bad user experience. Please see our [Linux installation instructions](https://github.com/ponylang/ponyc/blob/main/INSTALL.md#linux) for a list of Linux distributions we currently create ponyc packages for.

