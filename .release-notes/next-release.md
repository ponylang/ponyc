## Fix potential use-after-free on Windows during socket teardown

When a transient system error occurred during socket teardown on Windows, the runtime freed memory that its I/O thread could still reference. The runtime now retries the operation and, if the error persists, leaves the memory allocated rather than freeing it while references remain.

## Replace the runtime allocator

The runtime's pool allocator couldn't return freed memory to the operating system, reuse a large block on a thread that didn't free it, or re-carve memory from one size class for another. A program that passed large blocks between threads reserved fresh address space for every block it freed and grew without bound.

Every platform now uses a new allocator in which every piece of memory has an owning thread, following the design in [discussion #5735](https://github.com/ponylang/ponyc/discussions/5735). Memory comes from the operating system in large shared regions that threads carve into arenas; freed memory is reused across threads and size classes, and an emptied arena's physical memory goes back to the operating system while its address space is kept for reuse.

The previous allocator stays available behind a new build option:

```bash
cmake --preset release -DPONY_USES=pool_classic
```

Building with `address_sanitizer`, `valgrind`, or `pooltrack` now requires pairing with `pool_classic` (or `pool_memalign` for AddressSanitizer), since none of the three can track the new allocator's memory. `pool_retain` stops the classic pool returning memory to the operating system, so it requires `pool_classic` too. The build stops with an error saying so.

## Add the --ponymemoryprofile runtime option

`--ponymemoryprofile` picks where a program runs on the new allocator's trade between resident memory and throughput. The allocator makes that trade by how much freed memory each thread holds for immediate reuse and how promptly the rest returns to the operating system. It takes a number from 1 to 10: 1 returns memory quickly for the smallest footprint, 10 holds it for the most throughput, and each of the ten steps is a distinct setting. The default is 3 -- the scale has little room below it for less memory and much more above it for throughput, so the balanced default sits low on it.

```bash
./my-program --ponymemoryprofile=8
```

A program can also set it in code through the `RuntimeOptions` struct, the same as the other runtime options. The option affects only the new allocator; a program built with `pool_classic` ignores it.

## Change how scheduler scaling works

The runtime paused idle scheduler threads through one of three per-platform mechanisms — signals on Linux and the BSDs, pthread condition variables on macOS and on builds that passed `use=scheduler_scaling_pthreads`, a native event on Windows — and a paused thread waited for another thread to wake it. All three mechanisms are gone: on every platform, an idle scheduler thread now suspends on its own and periodically checks for work. Nothing wakes a suspended thread; anything sent to one is found at its next check.

The `scheduler_scaling_pthreads` build option selected the condition-variable mechanism on platforms where signals were the default, so it is gone too: a build that passes `use=scheduler_scaling_pthreads` now fails. Systematic testing no longer pairs with it — build with `use=systematic_testing` alone.

The signal mechanism used SIGUSR2, so the runtime reserved that signal on Linux and the BSDs: you could not handle SIGUSR2 through the `signals` package, and `Sig.usr2()` was a compile error on those platforms. The runtime no longer reserves it — `Sig.usr2()` now returns the signal number on Linux and the BSDs, and a program can subscribe to SIGUSR2 like any other signal. The runtime never reserved it on macOS, so nothing changes there.

In the `runtime_info` package, `Scheduler.sleeping_schedulers` is now `Scheduler.suspended_schedulers`, using the same word for an idle scheduler thread as the rest of the runtime. A program calling the old name will not compile; change the call to the new name.
## Fix pony-lint running out of memory on repos with many packages

Running `pony-lint` on a repository with many package directories — for example, a library with 35 example programs — used enough memory to crash the process. Memory grew with each package and was not released until the run finished, so a repo that needed about 1 GB for one package could need 18 GB for 37.

pony-lint now releases memory between packages. Peak usage stays near the cost of one compilation regardless of how many packages the repo contains.

## Readline preserves the in-progress line when browsing history

Browsing history with the up and down arrow keys in `Readline` no longer discards the line you were typing. Previously, pressing up replaced the current line with no backup, and pressing down at the newest entry cleared it. Edits made to a recalled history entry were also lost when navigating away and back.

The line you were typing reappears when you arrow back past the newest history entry. Edits to recalled entries stick while you browse and reset when you press Enter.

## Fix slow stdin reads from redirected files on Windows

On Windows, reading a file redirected into stdin (`program < file.txt`) was about nine times slower than necessary. The runtime now uses the same approach Linux and macOS use for redirected files, eliminating the overhead.

## Add a streaming JSON parser

`JsonTokenParser` now parses JSON incrementally, so you can parse input that arrives in pieces — over a socket, or a large file read in chunks — or that is too big to hold in memory at once. Feed it bytes with `feed()` as they arrive and it pushes tokens to your notifier as they complete, building no tree, so you control how much memory the parse uses: process each token and drop it and memory stays flat however large the document.

```pony
let parser = JsonTokenParser(notify)
parser.feed(chunk)?  // once per chunk, as bytes arrive
parser.finish()?     // when the input ends
```

When a string or key value fits within a single fed chunk with no escapes, it is a zero-copy view into that chunk rather than a fresh copy. `JsonReassembler` folds a token stream back into the same `JsonValue` the batch `JsonParser` builds, when you want one, and `JsonParseLimits` caps nesting depth and string and number length for untrusted input. `JsonParser.parse()` is unchanged.

## Change JsonTokenParser to carry values on its tokens

This is a breaking change to `JsonTokenParser`. Its tokens now carry their own value instead of exposing it through `parser.last_string` / `parser.last_number`, and it is driven with `feed()` / `finish()` instead of `parse()`.

Before:

```pony
let parser = JsonTokenParser(
  object is JsonTokenNotify
    fun ref apply(p: JsonTokenParser, token: JsonToken) =>
      match token
      | JsonTokenKey => use_key(p.last_string)
      | JsonTokenNumber =>
        match p.last_number
        | let i: I64 => use_int(i)
        | let f: F64 => use_float(f)
        end
      end
  end)
parser.parse(whole_document)?
```

After:

```pony
let parser = JsonTokenParser(
  object is JsonTokenNotify
    fun ref apply(p: JsonTokenParser, token: JsonToken) =>
      match token
      | let k: JsonTokenKey => use_key(k.value)
      | let n: JsonTokenNumber =>
        match n.value
        | let i: I64 => use_int(i)
        | let f: F64 => use_float(f)
        end
      end
  end)
parser.feed(whole_document)?
parser.finish()?
```

`JsonParser.parse()` is unchanged.
