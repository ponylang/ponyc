## Fix memory corruption of chopped Arrays and Strings

With the introduction of `chop` on `Array` and `String` a few years ago, a constraint for the memory allocator was violated. This resulted in an optimization in the realloc code of the allocator no longer being safe.

Prior to this fix, the following code would print `cats` and `sog`. After the fix, it doesn't corrupt memory and correctly prints `cats` and `dog`.

```pony
actor Main
  new create(env: Env) =>
    let catdog = "catdog".clone().iso_array()
    (let cat, let dog) = (consume catdog).chop(3)
    cat.push('s')
    env.out.print(consume cat)
    env.out.print(consume dog)
```
