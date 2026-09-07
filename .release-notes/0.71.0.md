## Make terminal raw mode opt-in and auth-gated

The runtime no longer puts stdin into pseudo-raw mode at startup. Programs that read from `env.input` without using `ANSITerm` now get normal cooked-mode input: echoed, line-buffered, with Ctrl-D interpreted as EOF.

`ANSITerm` now requires a `TerminalAuth` (derived from `AmbientAuth`) and sets raw mode itself on construction. It also restores the original terminal mode on dispose and re-applies raw mode after a suspend/resume (SIGCONT).

Before:

```pony
use "signals"
use "term"

actor Main
  new create(env: Env) =>
    let term = ANSITerm(SignalAuth(env.root), notify, env.input)
```

After:

```pony
use "signals"
use "term"

actor Main
  new create(env: Env) =>
    let term = ANSITerm(
      SignalAuth(env.root), TerminalAuth(env.root), notify, env.input)
```

For programs that need raw mode without `ANSITerm`, use `TerminalMode` directly:

```pony
use "term"

actor Main
  new create(env: Env) =>
    let auth = TerminalAuth(env.root)
    TerminalMode.set_raw(auth)
    // ... read raw input ...
    TerminalMode.restore(auth)
```

## Fix iftype capability narrowing inside generic return types

When a method on a generic class used `iftype` to narrow a type parameter's capability and returned a generic container parameterized by that type parameter, the compiler rejected the body with "function body isn't the result type."

For example, a method returning `MyBox[A]^` that produces `recover iso MyBox[A].create() end` inside an `iftype A <: Any val` branch was rejected.

This now compiles correctly. The fix applies to all capability constraints, including `#share`.

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

### `Randomness.end_span` is now partial

`end_span` errors when called with no open span, catching mismatched `start_span`/`end_span` calls at the point of misuse rather than silently ignoring them. Add `?` to every call site:

```pony
// Before
rnd.end_span()

// After
rnd.end_span()?
```

### Removed API

Each removed method either has a direct replacement or is no longer needed because shrinking is automatic.

#### `Generator.generate_value` → `Generator.generate`

`generate_value` returned only the value, discarding the shrink iterator. `generate` does the same thing — it's the only generate method now.

```pony
// Before
let value = my_gen.generate_value(rnd)?

// After
let value = my_gen.generate(rnd)?
```

#### `Generator.generate_and_shrink` — delete the shrink plumbing

`generate_and_shrink` returned `(T^, Iterator[T^])` — a value and its shrink iterator. Custom generators that composed multiple sub-generators had to destructure each result and manually zip the shrink iterators together. All of that is gone. Call `generate` on each sub-generator and combine the values directly.

The before/after example in "Custom generators" above shows this migration.

#### `Generator.value_iter` and `Generator.shrink` — delete entirely

`value_iter` took a value and returned an iterator of progressively shrunk versions. `shrink` did the same thing. Both were building blocks for composing shrink iterators by hand. The framework now shrinks by replaying generators against mutated choice sequences, so there is no user-facing shrink API and no code to replace these calls with — delete them.

```pony
// Before — manually iterating shrunk values
let shrinks: Iterator[U32^] = my_gen.value_iter(failing_value)
for shrunk in shrinks do
  // test each shrunk value
end

// After — nothing. The framework does this internally.
```

#### `GenerateResult`, `ValueAndShrink` type aliases — delete entirely

These typed the old return value of `generate`. `GenerateResult[T]` was `(T^ | (T^, Iterator[T^]))`. With `generate` returning `T^` directly, neither alias is needed.

#### `Generator.iter` and `Generator.value_and_shrink_iter` — delete entirely

`iter` returned an `Iterator[GenerateResult[T]]` and `value_and_shrink_iter` returned an `Iterator[ValueAndShrink[T]]`. Both depended on removed types. Delete calls to them.

#### `do_shrink` parameter on `Generators.one_of` and `Generators.unit` — delete the argument

`one_of` and `unit` no longer take a `do_shrink` parameter. Remove it from the call site; shrinking is handled automatically.

```pony
// Before
Generators.one_of[Color]([Blue; Green; Pink] where do_shrink = false)

// After
Generators.one_of[Color]([Blue; Green; Pink])
```

#### `PropertyParams.max_shrink_rounds` → `PropertyParams.max_shrink_reductions`

The parameter that limits shrinking effort has been renamed and its default changed from 10 to 100. The old name counted shrink pass rounds; the new name counts accepted reductions, which is what the convergence loop actually tracks.

```pony
// Before
PropertyParams(where max_shrink_rounds' = 20)

// After
PropertyParams(where max_shrink_reductions' = 20)
```

