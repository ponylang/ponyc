## PonyCheck: choice-sequence-based internal shrinking

PonyCheck's shrinking system has been replaced. Generators no longer provide shrink logic — the framework records every random draw during generation and replays mutated sequences to find smaller failing inputs. Custom generators become simpler because they only produce a value; shrinking is automatic.

### Custom generators

The `generate` method now returns `T^` instead of `GenerateResult[T]`. The old shrink-iterator machinery (`generate_and_shrink`, `value_iter`, `GenerateResult`, `ValueAndShrink`) is removed.

Before:

```pony
object is GenObj[MyPony]
  fun generate(rnd: Randomness): GenerateResult[MyPony] ? =>
    (let name, let name_shrinks) =
      name_gen.generate_and_shrink(rnd)?
    (let score, let score_shrinks) =
      score_gen.generate_and_shrink(rnd)?
    let result = MyPony(consume name, consume score)
    let shrinks =
      Iter[String^](name_shrinks)
        .zip2[U64^](score_shrinks)
        .map[MyPony^]({(z) =>
          (let n, let s) = consume z
          MyPony(consume n, consume s)
        })
    (consume result, shrinks)
end
```

After:

```pony
object is GenObj[MyPony]
  fun generate(rnd: Randomness): MyPony^ ? =>
    let name = name_gen.generate(rnd)?
    let score = score_gen.generate(rnd)?
    MyPony(consume name, consume score)
end
```

### Randomness draw methods are now partial

All draw methods on `Randomness` (`u8`, `u16`, `u32`, `u64`, `u128`, `usize`, `ulong`, `i8`, `i16`, `i32`, `i64`, `i128`, `isize`, `ilong`, `f32`, `f64`, `bool`, `shuffle`) are now partial. Add `?` to every call site:

```pony
// Before
let n = rnd.u32(0, 100)

// After
let n = rnd.u32(0, 100)?
```

In plain mode (a `Randomness` you construct directly), draws never error.

### Removed API

- `GenerateResult`, `ValueAndShrink` type aliases
- `Generator.generate_and_shrink`, `Generator.generate_value`, `Generator.value_iter`, `Generator.shrink`
- The `do_shrink` parameter on `Generators.one_of`
