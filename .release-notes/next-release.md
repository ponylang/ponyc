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

## Report a process's exit even when its pipes stay open

`ProcessMonitor` reported a child's exit status only once the child's stdout and stderr had both reached end-of-file. A pipe reaches end-of-file only when every process holding its write end has closed it, so a child that left a grandchild holding its stdout or stderr open, or that exited while you still held its stdin open, was reported late or never, and the program could hang. The exit is now detected directly, from the operating system, and reported promptly regardless of what still holds the pipes.

## ProcessMonitor.dispose no longer risks signaling an unrelated process

Disposing a `ProcessMonitor` after its child had already exited could send a signal to whatever process the operating system had since assigned the child's old process id. It no longer signals a child that has been reaped.

## ProcessMonitor no longer leaks file descriptors when a process fails to start

A process that failed to start left the pipes that had been opened for it open. They are now closed.

## Starting a process returns a result

Starting a process now returns either a `ProcessMonitor` or a `ProcessError`, instead of always giving you a monitor. Replace the constructor with `StartProcess`:

Before:

```pony
let pm = ProcessMonitor(sp_auth, bp_auth, consume notifier, path, args, vars)
```

After:

```pony
match StartProcess(sp_auth, bp_auth, consume notifier, path, args, vars)
| let pm: ProcessMonitor => // a live child is running
| let err: ProcessError  => // never started; err says why
end
```

Failures that used to arrive through `ProcessNotify.failed` — no execute permission, a missing executable, and, on Linux, a kernel older than 5.3 — are now returned by `StartProcess` instead. The `ExecveError` that meant both "the file is missing" and "execve failed in the child" is split; the missing-file case is now `ExecutableNotFound`.

## ProcessMonitor requires Linux 5.3 or newer

On Linux, detecting a child's exit now uses `pidfd_open`, which requires kernel 5.3 or newer. On an older kernel, `StartProcess` returns an `UnsupportedKernel` error rather than starting a process it cannot monitor.
## Fix FilePath.from allowing paths outside the base directory

`FilePath.from` and `FilePath.join` are meant to keep the resulting path within the directory that the base `FilePath` grants access to. Two kinds of relative path were accepted despite pointing outside it, producing a `FilePath` that carried the base's capabilities.

The first was a sibling whose name began with the base directory's name. Given a `FilePath` for `/srv/app`, `base.join("../app-backup/secret")` was accepted and named a file under `/srv/app-backup`, outside `/srv/app`. The second was a path containing an embedded NUL byte: it was accepted, and the operating system, which stops reading a path at the first NUL, then acted on a shorter path naming a different file. `Directory` operations that take a relative path were affected in the same way.

Both are now rejected with an error. A path is accepted only when it is the base path itself or a path below it, and never when it contains a NUL byte.

Containment is still checked on the path text and does not resolve symlinks, so a symlink within the directory can still lead outside it.

## Fix trait default method bodies failing to compile with aliased use packages

When a trait defined a default method body that referenced a package through an aliased `use` statement, and a type in a different file implemented the trait without its own alias for the same package, the compiler would report "can't access package." The alias lived in the trait's module scope and was not carried along when the default body was copied into the implementing type. Unaliased `use` imports were not affected.

```pony
// greeting.pony
use collections = "collections"

trait Greeting
  fun hello() =>
    // This body is copied into any type that implements Greeting.
    // The reference to `collections` failed when the implementing
    // type was in a different file without its own alias.
    let hi = collections.Map[String, String]
    hi.insert("hello", "world!")
```

```pony
// main.pony — no `use collections` needed here
actor Main is Greeting
  new create(env: Env) =>
    hello()
```

## Fix TCP and UDP sockets being closed when the system runs low on socket buffers

Under heavy load the operating system can momentarily run out of buffer space while a socket is sending. Previously the runtime treated this as a fatal error: a `TCPConnection` closed the connection, dropping data that had not yet been delivered, and a `UDPSocket` closed entirely because a single datagram could not be queued. The condition is transient, so it is now handled the way a full send buffer already is — a `TCPConnection` pauses sending and resumes once space is available, and a `UDPSocket` drops the datagram and keeps running. This was most visible on macOS under high connection churn.

## Fix compiler crash when combining traits with abstract and default method

When a type implemented two traits that declared the same method — one without a body and one with a default body — the compiler crashed instead of compiling the program. The crash depended on the order of traits in the provides list: it occurred when the trait without a body appeared before the trait with a default body.

```pony
// no_body.pony
trait HasHello
  fun hello(env: Env)

// greeting.pony
trait Greeting
  fun hello(env: Env) =>
    env.out.print("hello!")

// main.pony — crashed with `(HasHello & Greeting)`, compiled fine with `(Greeting & HasHello)`
actor Main is (HasHello & Greeting)
  new create(env: Env) =>
    hello(env)
```

## Add the style/testlist-nodoc pony-lint rule

`style/testlist-nodoc` flags types that provide `TestList` without a `\nodoc\` annotation. `TestList` implementations exist only to register tests with PonyTest and don't belong in generated documentation.

```pony
// Flagged — missing \nodoc\
primitive _MyTests is TestList
  new make() => None
  fun tag tests(test: PonyTest) => None

// Clean
primitive \nodoc\ _MyTests is TestList
  new make() => None
  fun tag tests(test: PonyTest) => None
```

The rule is on by default. Disable it with `--disable style/testlist-nodoc` or in `.pony-lint.json`.

## Fix stdin end-of-input and error handling

On Windows, a program reading stdin from the console could never reach end of input. Typing Ctrl+Z — the Windows end-of-input key — delivered the byte 0x1A to the program instead of ending the stream. The program ran until killed. Ctrl+Z now ends console input.

Separately, a failed read from stdin was not reported to the program on any platform. `InputNotify` now has a `read_failed` method, called before `dispose` when the stream ends because of a read failure:

```pony
env.input(
  object iso is InputNotify
    fun ref apply(data: Array[U8] iso) =>
      // process data

    fun ref read_failed() =>
      env.exitcode(1)

    fun ref dispose() =>
      // cleanup
  end)
```

Existing programs that do not override `read_failed` behave as before.

## Add read_failed to InputNotify

`InputNotify` has a new method, `read_failed`, called before `dispose` when a read from the stream fails. Code that structurally matches `InputNotify` without `is InputNotify` will not compile until `read_failed` is added or `is InputNotify` is declared:

Before:

```pony
let notify = object iso
  fun ref apply(data: Array[U8] iso) => // ...
  fun ref dispose() => // ...
end
env.input(consume notify)
```

After:

```pony
let notify = object iso is InputNotify
  fun ref apply(data: Array[U8] iso) => // ...
  fun ref dispose() => // ...
end
env.input(consume notify)
```

Code that already uses `is InputNotify` is unaffected — it picks up the default no-op.

## Fix compiler crash with multiple traits sharing a method with a default body

When a type implemented multiple traits declaring the same method and one of those traits provided a default body, the compiler could crash.

```pony
trait Abstract1
  fun hello(env: Env)

trait Abstract2
  fun hello(env: Env)

trait Concrete
  fun hello(env: Env) =>
    env.out.print("hello")

actor Main is (Abstract1 & Abstract2 & Concrete)
  new create(env: Env) =>
    hello(env)
```

## Exempt triple-quoted string literals from the 80-column lint rule

pony-lint's `style/line-length` rule now skips lines inside triple-quoted string literals that are not docstrings. Triple-quoted strings used as data — JSON templates, inline test fixtures, multi-line format strings — exist for readability, and forcing them to wrap at 80 columns defeats their purpose. Docstring prose is still checked.

## pony-lint line-length rule narrows unbreakable-word exemption

The `style/line-length` rule now exempts a line only when one of the first two space-delimited words crosses column 80. A long word that appears later on the line — after other content — is flagged, because the content before it can go on a separate line.

This replaces the previous string-literal exemption. Lines like `// https://very-long-url` are exempt (the URL is the second word and cannot be shortened by breaking the line). Lines like `// some text https://very-long-url` are flagged — "some text" and the URL can go on separate comment lines, and the URL-only line is then exempt on its own.

## Fix pony-lint false positive on identifiers with multiple trailing primes

pony-lint flagged identifiers like `path''` as naming violations because it only stripped one trailing prime before checking the name. An identifier with two or more primes kept the extras and failed the snake_case or CamelCase check. pony-lint now strips all trailing primes before validating.

## Fix pony-lint false positive on partial operators

pony-lint's operator-spacing rule flagged partial arithmetic operators (`*?`, `+?`, etc.) as missing a space after the base operator. The `?` that makes the operation partial shares the same AST token as the non-partial form, so the rule saw `?` where it expected a space.

