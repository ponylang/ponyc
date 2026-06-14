# cshim

This program calls C code that `ponyc` compiles itself. A `.c` file placed in a package's directory (next to its `.pony` files) is a *C shim*: `ponyc` compiles it with the embedded clang and links the resulting object into the program. There is no separate C build step, no shared library, and no `use "lib:..."` directive — compare with the [ffi-callbacks](../ffi-callbacks/) and [ffi-struct](../ffi-struct/) examples, which link prebuilt libraries.

ponyc's own headers are on the include path by default (resolved relative to the running compiler, like the standard library), so a shim can `#include <pony.h>` and use runtime APIs with no configuration.

Two `use` schemes configure how a package's shims are compiled:

- `use "cdefine:NAME"` or `use "cdefine:NAME=VALUE"` defines a C preprocessor macro (clang's `-D`). Defining the same macro name twice in one package is a compile error.
- `use "cinclude:PATH"` adds an include search directory (clang's `-I`). Relative paths resolve against the package's directory.

Both apply only to the package that declares them; a directive in a package with no `.c` files is an error (the shim it was meant for is usually in another package).

Both schemes accept guards for platform-specific flags, e.g. `use "cdefine:USE_EPOLL" if linux`. The shim sources themselves are always compiled on every platform; a platform-specific `.c` file wraps its whole body in `#ifdef` so it compiles to an empty object elsewhere.

## How to Compile

With a minimal Pony installation, in the same directory as this README file run `ponyc`. The shim is compiled and linked automatically:

```console
ponyc
...
Compiling C shims
Generating
...
Linking ./cshim
```

## How to Run

Once `cshim` has been compiled, in the same directory as this README file run `./cshim`. You should see the macro value from `cdefine:` and the constant from the header found via `cinclude:`:

```console
$ ./cshim
answer from the shim: 42
version from the header: 7
```

## Program Modifications

Change the `cdefine:` value and rebuild — the shim picks up the new macro without any C build configuration. Then try defining `CSHIM_ANSWER` a second time with a different value and note the compile error: a macro name may only be defined once per package.

For a bigger change, add a second `.c` file with a new function and call it over FFI. Shim objects are linked directly (not archived), so two shims defining the same symbol produce a duplicate-symbol link error — try it by defining `cshim_answer` in both files.
