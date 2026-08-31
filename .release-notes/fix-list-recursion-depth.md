## Fix stack overflow (SEGV) in persistent List from unbounded recursion

Most persistent `List` operations caused a stack overflow at ordinary list sizes.

```pony
use "collections/persistent"
use mut = "collections"

actor Main
  new create(env: Env) =>
    var l: List[USize] = Lists[USize].empty()
    for i in mut.Range(0, 50_000) do l = l.prepend(i) end
    env.out.print(l.map[USize]({(x) => x * 2 }).size().string())
```

```
Segmentation fault (core dumped)
```

The length that crashed depended on the stack size and the build, so the same program could work on one machine and crash on another. In a debug build with an 8 MB stack, `map`, `filter`, and `concat` crashed at around 50,000 elements; `apply` and `Lists.eq` at around 200,000 even in a release build.

This has been fixed. The example above now prints `50000`. No signatures changed.
