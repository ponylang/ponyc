## Don't set ld.gold as the default Linux linker

Previously, we were setting ld.gold as our default linker on Linux. If you wanted to use a different linker, you had to override it using the ponyc option `--link-ldcmd`.

ld.gold was contributed to the GNU project many years ago by Google. Over the years, Google has moved away from supporting it and to investing in LLVM and other tools. ld.gold has no active maintainers at this point in time and has been removed from the most recent version of binutils.

It is no longer reasonable going forward to set gold as the default Linux linker as many Linux distributions will not have it easily available. We have switched to using the default Linux linker on the given system. On most systems, that is ld.bfd unless it has been configured otherwise. If you would like to use a different linker you can do the following as part of ponyc command line:

Use gold:

- `ponyc --link-ldcmd=gold`

Use mold:

- `ponyc --link-ldcmd=mold`
