## Compile C shims alongside Pony

Sometimes the easiest way to use a C library from Pony is a small piece of C: a wrapper that flattens a macro into a function, fixes up a calling convention, or adapts a struct-heavy API into something FFI-friendly. Until now that meant setting up a second build system to compile the C and a `use "lib:..."` directive to link it.

Now ponyc compiles the C for you. A `.c` file placed in a package's directory (next to its `.pony` files) is a *C shim*: ponyc discovers it, compiles it with an embedded copy of clang, and links the object into your program. No Makefile, no separate compiler invocation, no `use "lib:..."`.

```pony
// main.pony
use "cdefine:SHIM_ANSWER=42"

use @shim_answer[I32]()

actor Main
  new create(env: Env) =>
    env.out.print(@shim_answer().string())
```

```c
// shim.c, in the same directory
#include <stdint.h>

int32_t shim_answer(void)
{
  return SHIM_ANSWER;
}
```

Two new `use` schemes configure how a package's shims are compiled. `use "cdefine:NAME"` or `use "cdefine:NAME=VALUE"` defines a C preprocessor macro (clang's `-D`), and `use "cinclude:PATH"` adds an include search directory (clang's `-I`), with relative paths resolved against the package's directory. Both apply only to the package that declares them, and both accept guards: `use "cdefine:USE_EPOLL" if linux`. Defining the same macro name twice in one package is an error, whether or not the values match — platform-specific values belong behind guards, where only the active target's definition counts. Note that `cdefine:` is a C preprocessor macro for shims only; it is unrelated to ponyc's own `--define`/`-D` flag, which drives Pony's `ifdef`.

The shim sources themselves have no per-file guard: every `.c` in the package directory is compiled on every platform. A platform-specific shim wraps its whole body in `#ifdef` so it compiles to an empty object elsewhere.

Shim objects are linked directly, not archived into a library first. That has two consequences worth knowing. First, C constructors (`__attribute__((constructor))`) in shims run at program start, like any directly linked object. Second, two shims defining the same symbol fail the link loudly with a duplicate-definition error — but a shim that defines a symbol also provided by a `use "lib:..."` library silently wins, and the library's version is never pulled in.

That last point is the migration warning: if you previously compiled a `.c` next to your Pony code into a library by hand and linked it with `use "lib:..."`, ponyc now also compiles that file as a shim, and the shim object silently shadows your hand-built library. Either delete the manual build and the `use "lib:..."` directive (the shim path replaces both), or move the `.c` out of the package directory (a subdirectory works — only the package's top directory is scanned).

C compile errors are reported like any other ponyc error, with the file, line, and column. Shims are recompiled on every build; their objects live in the output directory during the build and are cleaned up after a successful link. Because ponyc now carries clang inside it, the compiler binary is noticeably larger than before.
