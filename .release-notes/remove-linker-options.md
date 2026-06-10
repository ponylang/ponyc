## Remove the `--linker` and `--link-ldcmd` command line options

ponyc now links every supported configuration with its embedded LLD linker, so the options that selected an external linker have been removed. `--link-ldcmd` already had no effect on the embedded linker, and `--linker` was an escape hatch back to the external linker, which no longer exists.

If you used `--linker` to link an additional library, declare it in your Pony source instead.

Before:

```
ponyc --linker="ld -lFoo"
```

After:

```pony
use "lib:Foo"
```

`use "path:..."` adds a library search path the same way. There is no longer a way to pass arbitrary linker flags directly; if your build relies on that, let us know in [this discussion](https://github.com/ponylang/ponyc/discussions/5448).
