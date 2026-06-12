"""
A program that calls C code compiled by ponyc itself: the `answer.c` shim
sits next to this file, so ponyc discovers it, compiles it with the
embedded clang, and links the object into the program. There is no
separate C build step and no `use "lib:..."` directive.

The two `use` schemes configure the shim's compilation: `cdefine:` sets a
C preprocessor macro (clang's -D) and `cinclude:` adds an include search
directory (clang's -I), resolved relative to this package's directory.
"""
use "cdefine:CSHIM_ANSWER=42"
use "cinclude:./include"

use @cshim_answer[I32]()
use @cshim_version[I32]()

actor Main
  new create(env: Env) =>
    env.out.print("answer from the shim: " + @cshim_answer().string())
    env.out.print("version from the header: " + @cshim_version().string())
