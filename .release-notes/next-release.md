## Fix compiler crash

Previously the following code would cause the compiler to crash. Now it will display a helpful error message:

```pony
struct FFIBytes
  var ptr: Pointer[U8 val] = Pointer[U8].create()
  var length: USize = 0

  fun iso string(): String val =>
    recover String.from_cpointer(consume FFIBytes.ptr, consume FFIBytes.length) end

actor Main
  new create(env: Env) =>
    env.out.print("nothing to see here")
```

## Add prebuilt ponyc binaries for Ubuntu 24.04

We've added prebuilt ponyc binaries specifically made to work on Ubuntu 24.04.

You can opt into using the Ubuntu binaries when using ponyup by running:

```bash
ponyup default ubuntu24.04
```

