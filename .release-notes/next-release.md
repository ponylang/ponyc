## Fedora 39 added as a supported platform

We've added Fedora 39 as a supported platform. We'll be building ponyc releases for it until it stops receiving security updates at the end of 2024. At that point, we'll stop building releases for it.

## Add support for MacOS on Apple Silicon

Back in August of 2023, we had to drop MacOS on Apple Silicon as a supported platform as we had no Apple Silicon test and build environment. We lost our existing environment when we had to migrate off of CirrusCI.

GitHub recently announced that they now have Apple Silicon MacOS machines so, we are back in business and MacOS on Apple Silicon is once again a supported platform.

## Fix for potential memory corruption in `Array.copy_to`

`Array.copy_to` did not check whether the source or destination Arrays had been initialized or whether the requested number of elements to be copied exceeded the number of available elements (allocated memory). These issues would result in potential dereferencing of a null pointer or attempts to access unallocated memory.

Before this fix, the following code would print `11` then `0`:

```pony
actor Main
  new create(e: Env) =>
    var src: Array[U8] = [1]
    var dest: Array[U8] = [11; 1; 2; 3; 4; 5; 6]

    try
      e.out.print(dest(0)?.string())
    end
    src.copy_to(dest, 11, 0, 10)

    try
      e.out.print(dest(0)?.string())
    end
```

After the fix, this code correctly prints `11` and `11`.

