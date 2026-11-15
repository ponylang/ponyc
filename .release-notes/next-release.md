## Fix C shims that include ponyassert.h

C shims that include `ponyassert.h` now compile with an installed compiler. Compilation previously failed because a required header was missing from the installation.

## Apply runtime tracing category changes to ASIO

Runtime tracing category changes now apply to the ASIO thread as well as worker and pinned schedulers. Previously, ASIO retained its startup categories, so enabling a category at runtime could omit ASIO events and disabling or replacing a category could leave ASIO events enabled.

## Fix compilation failure on OpenBSD and DragonFly BSD

ponyc failed to compile on OpenBSD and DragonFly BSD because the runtime's `ponyint_futimesat` wrapper called `futimesat`, which only exists on Linux and FreeBSD. The wrapper now uses `utimensat`, available on all supported platforms.

## Rename `ponyint_futimesat` to `ponyint_utimensat`

`ponyint_futimesat` has been renamed to `ponyint_utimensat`. Code that calls it via FFI must update the function name.

## Add Nix flake package for x86-64 Linux

ponyc can now be built and installed as a Nix flake package. `nix build .#ponyc` produces an installation that includes ponyc and all language tools (pony-lsp, pony-lint, pony-doc, pony-dep, pony-agent-server). Programs compiled by the installed ponyc link against libraries in the Nix store. A development shell for building ponyc from source is available via `nix develop`.

## Fix MultipartFormData header injection via CR and LF in names and content types

`MultipartFormData` wrote CR and LF from field names, filenames, and content types directly into part headers. An attacker who controlled a name, filename, or content type passed to `field()` or `file()` could inject extra headers into a part or end the part's header block early, moving attacker-controlled text into the part body.

Names and filenames are now percent-encoded following WHATWG (`%22` for `"`, `%0D` for CR, `%0A` for LF). `file()` rejects content types that contain CR or LF.

## MultipartFormData uses WHATWG percent-encoding; file() is now partial

`MultipartFormData` name and filename escaping has changed from backslash escaping (`\"`, `\\`) to WHATWG percent-encoding (`%22` for `"`, `%0D` for CR, `%0A` for LF). Backslash is no longer escaped — it passes through unchanged, matching what browsers send.

`file()` is now partial. It errors if the content type contains CR or LF, which would allow header injection. Callers must handle the error:

Before:

```pony
form.file("f", "photo.jpg", "image/jpeg", data)
```

After:

```pony
form.file("f", "photo.jpg", "image/jpeg", data)?
```

## Fix `FilePath.from` letting `..` escape a `//` base path

`FilePath.from` accepted `..` paths that escaped the base directory when the base path started with exactly `//` and had one or two components after it (e.g., `//srv/www`). On Linux, where `//` resolves to `/`, the returned `FilePath` carried the base's capabilities but pointed outside the base. Every `Directory` method that takes a relative path had the same escape.

Paths starting with `//` followed by fewer than two valid components (e.g., `//host/` with a trailing slash and no share name) were also accepted as network volumes. They are now rejected, and the leading `//` collapses to `/` during cleaning.

`FilePath.from` now rejects any cleaned path that still contains a `..` component.

## Fix TLS hostname verification accepting prefix matches

`X509.valid_for_host` accepted a certificate whose name was a byte-prefix of the hostname. A certificate for `example.co` passed verification for `example.com`, and `*.example.co` passed for `www.example.com`. The wildcard branch also accepted `*.com` for `foo.com` and `*.example.com` for `.example.com` (empty label).

The function also checked the common name when DNS SANs were present, and matched IP hosts against DNS SANs instead of iPAddress SANs only.

All of these are now rejected. Hostname matching follows RFC 6125: exact names require equal length, wildcards match a single non-empty label with at least two domain levels after `*.`, the CN is skipped when DNS SANs exist, and IP hosts match only iPAddress SANs.

Code that relies on `X509.valid_for_host` or `X509.all_names` through the public API is unaffected — `all_names` still returns all certificate names as before. Code that called `valid_for_host` and happened to connect through a verifying client to a server whose certificate was only a prefix match will now see `SSLAuthFail` where it previously saw `SSLReady`.

## Fix set and map generators silently returning fewer than `from` elements

The `set_of`, `set_is_of`, `map_of`, `map_is_of`, `persistent_set_of`, `persistent_set_is_of`, `persistent_map_of`, and `persistent_map_is_of` generators could return a collection smaller than `from` when the element generator couldn't produce enough distinct values. The generators now error instead, which the property runner retries and then fails visibly.

## Fix PonyTest regression database collisions between properties

The PonyTest regression database mapped different property names to the same file in two cases: names containing a literal `#` (e.g., `x#2` collided with the second stored failure of `x`), and names differing only in case (`Foo` and `foo` mapped to the same file). A property could replay another property's stored regressions, report false "stored regression still fails" errors, or delete another property's regression files.

Property names are now encoded so that `#` and uppercase letters produce distinct filenames.

## PonyTest regression database encoding has changed

Existing `.ponytest/` regression files from the old encoding are not migrated. They become orphaned, and any stored regressions will be rediscovered on the next failing run. Delete your `.ponytest/` directory to clean up orphaned files.

## Fix `Path.rel` producing wrong results for cross-volume `//` paths on non-Windows

`Path.rel` silently returned a wrong relative path when called with two `//` network paths on different volumes (e.g., `Path.rel("//host1/share1/a", "//host2/share2/b")`). Joining the result back produced a path on the first volume, not the second. The call now correctly errors when the two paths have different volumes, matching the existing behavior on Windows.

## Fix `Generators.utf32_codepoint_string` hanging on all-surrogate code-point generators

`Generators.utf32_codepoint_string` looped forever when its code-point generator produced only surrogates (0xD800-0xDFFF). A generator that produced mostly surrogates was slow in proportion to the surrogate fraction. The loop now errors after 100 consecutive invalid draws, matching `Generator.filter`.

`Generators.unicode` and `Generators.unicode_bmp` are unaffected — their code-point generators produce only valid values.

## `Generators.utf32_codepoint_string` now rejects values above 0x10FFFF

`Generators.utf32_codepoint_string` previously passed values above 0x10FFFF to `String.push_utf32`, which silently replaced them with U+FFFD. These values are now rejected and redrawn, the same as surrogates. After 100 consecutive rejected draws for a single code point, the generator errors.

Code that passed a code-point generator whose range extends above 0x10FFFF will now see errors where it previously produced strings containing U+FFFD. To generate valid Unicode strings, use `Generators.unicode` or `Generators.unicode_bmp`, which produce only valid code points.

## Fix NaN target score replacing the best entry in PonyCheck's target pool

`PropertyHelper.target` accepted NaN scores, which silently replaced the best entry in the target pool. Once a NaN was stored, the next score of any value replaced it, losing the original best choice sequence. A scoring function that returned `0/0` or the difference of two infinities was enough to break targeting for that label.

A NaN score now fails the property with a diagnostic message naming the label. Infinite scores are unaffected — they compare normally.

## Fix Generators.f64 and f32 producing NaN with infinite bounds

`Generators.f64` and `Generators.f32` returned NaN when either bound was infinite. With one infinite bound, roughly half the output was NaN and the rest was the infinite bound itself; no finite value was ever produced. `Generators.f64` and `f32` now error on infinite bounds, the same as they already do on NaN bounds.

`Randomness.f64` and `Randomness.f32` had the same underlying problem. Because these methods are non-partial, infinite bounds are clamped to the largest finite value rather than raising an error.

Targeted testing (`PropertyHelper.target`) over `Generators.f64(-F64.max_value(), F64.max_value())` also produced NaN. Full-range float generators now work correctly with targeted testing.

## Fix `XorOshiro128Plus(0, 0)` and `XorShift128Plus(0, 0)` producing only zeros

`XorOshiro128Plus(0, 0)`, `XorOshiro128StarStar(0, 0)`, and `XorShift128Plus(0, 0)` returned 0 from every call to `next()`. An all-zero state is a fixed point of xorshift-family generators: every output is zero and the state never advances. The constructors now remap `(0, 0)` through SplitMix64.

## Fix pony_test `Randomness` producing degenerate sequences for seed 0

`Randomness(seed)` passed the seed directly as half the PRNG state with the other half zero. `Randomness(0)` produced a generator stuck at zero, and small seeds produced biased early output. The constructor now seeds through SplitMix64, producing well-distributed state from any input. Existing `.ponytest/` regression files are unaffected — they store choice sequences, not seeds — but a pinned seed value produces a different sequence than before.

## Fix identical NaN property failures reported as distinct

Property tests using float generators that produce NaN reported each failure as a new one, even when the failures were identical. The equality check compared floats with `==`, which returns false when either operand is NaN, so identical failure sequences containing NaN were never recognized as duplicates.

Float choices are now compared by their bit patterns. Two choices with the same bits are equal regardless of NaN, and `-0.0` and `0.0` are now distinct because they have different bits.

## Fix property-test shrinker counting identical candidates as reductions

The property-test shrinker accepted candidates identical to the current baseline, counting each as a reduction. When a shrink pass produced the same value repeatedly, the budget drained without the value changing, and the "after N shrinks" count in the failure message reflected attempts rather than actual reductions.

The shrinker now rejects candidates identical to the current baseline and terminates stalled binary searches that cannot make progress.

## Fix `Generators.frequency` over-weighting the first entry

`Generators.frequency` in `pony_test` picks a value from a weighted set of generators. It over-weighted its first entry — a first entry with weight 0 was chosen when it should never have been, and all other entries were skewed toward the first entry by one extra share of the total weight.

Each entry is now chosen with probability `weight / weight_sum`. Entries with weight zero are never chosen. When all weights are zero, `generate` errors.

Saved regression inputs that used `frequency` may replay differently. Delete `.ponytest/` and let regressions be rediscovered on the next failing run.

## Fix collection generator length distribution regression

Collection generators (`array_of`, `seq_of`, `set_of`, `map_of`, `byte_string`, `utf32_codepoint_string`, and their variants) produced sizes averaging 1 above `from`, regardless of `to`. `array_of(u8(), 0, 100)` produced an empty array half the time and almost never produced one longer than 14 elements. The `to` bound had no practical effect on the distribution.

Sizes are now uniformly distributed on `[from, to]`, matching the behavior through 0.70.x.

`Randomness.biased_bool(numerator, denominator)` is now available for custom generators that use the boolean-per-element pattern and need to control their size distribution.

```pony
let cont = rnd.biased_bool(remaining, remaining + 1)
```

## Fix float shrinking stopping short of the target

The property-testing shrinker binary-searched float choices toward zero by halving the distance each step. Three things went wrong.

Finite values took about 1050 accepted steps to reach zero, against the default budget of 100. Shrinking stopped at a tiny nonzero value, and distinct failures that should have deduplicated into one were reported separately.

Infinite values produced infinite midpoints (`inf / 2 = inf`), so the search made no progress and consumed the entire budget.

NaN values were skipped entirely. Pony's ordered `!=` returns false when either operand is NaN, so the entry check `value != target` evaluated to false and the shrinker never tried to shrink the NaN choice.

The shrinker now tries the target value directly before binary-searching. When the property still fails at the target, shrinking reaches it in one step. For infinite and NaN baselines, where the binary search cannot make progress, the direct attempt is the only path that works. The binary search also stops when it computes a non-finite midpoint.

## Fix http_client sending bare CR and LF from Location headers into redirect requests

The response parser in `http_client` kept bare CR, bare LF, and NUL bytes inside parsed header values. A server's `Location` header containing a bare LF could inject extra headers into the redirect request the client sent next, because the LF ended the request line and the bytes after it became headers on the wire.

The parser now replaces bare CR, bare LF, and NUL with SP in header field values, header names, and the status line reason phrase.

## Fix Format.int with FormatUTF32 ignoring width, fill, and align

`Format.int` with `FormatUTF32` returned the bare character and ignored `width`, `fill`, and `align`. Padding and alignment are now applied.

## Fix `Path.rel` returning wrong results when the base is `"."` or `""`

`Path.rel(".", "a")` returned `"../a"` instead of `"a"`. Any base that cleaned to `"."` (including `""`) produced the same wrong result — the relative path gained a spurious `../` prefix. The documented contract `Path.join(to, Path.rel(to, target)) == target` failed for every target other than `"."` itself.

## Reject dot segments in `URIBuilder.append_path_segment`

`URIBuilder.append_path_segment` now rejects `.` and `..` segments. `build()` returns `InvalidDotSegment` when either was appended.

No percent-encoding preserves `.` or `..` as literal path segments — RFC 3986 §6.2.2.2 normalizes `%2E` back to `.` before dot-segment removal runs. An appended `..` caused `NormalizeURI` and `ResolveURI` to remove the preceding segment, which is path traversal when segments come from untrusted input.

`set_path` is unaffected — it accepts the full path as-is, and dot segments in a complete path are the caller's responsibility.

`URIBuildError` gains the `InvalidDotSegment` variant. Exhaustive matches on `URIBuildError` must add a case for it:

Before:

```pony
match \exhaustive\ builder.build()
| let u: URI val => use(u)
| let e: URIBuildError val => handle(e)
end
```

After (if matching individual error types):

```pony
match \exhaustive\ builder.build()
| let u: URI val => use(u)
| let _: InvalidScheme val => // ...
| let _: InvalidDotSegment val => // ...
| let e: URIParseError val => // ...
end
```

Callers who match `URIBuildError` as a single arm are unaffected.

## Fix cli `CommandSpec` accepting empty command names and names starting with `-`

`CommandSpec.parent` and `CommandSpec.leaf` accepted empty command names and names starting with `-`. A subcommand with either kind of name could not be invoked from a command line: the parser treated `-x` as a short option and an empty name matched nothing.

Both constructors now error on empty names and names starting with `-`.

## cli `CommandSpec` now requires command names to start with a letter

`CommandSpec.parent` and `CommandSpec.leaf` accepted command names starting with a digit or underscore (e.g., `1abc`, `_a`). These names worked as subcommands but didn't match the documented grammar (`alphanum_word ::= alphachar(alphachar | numchar | '_' | '-')*`).

Both constructors now enforce the documented grammar: the first character must be a letter (`[A-Za-z]`). Programs that create a `CommandSpec` with a name starting with a digit or underscore will get an error from the partial constructor where they didn't before.

## Fix JSONPath parser accepting integers outside RFC 9535 range

The JSONPath parser accepted integers outside the ±(2^53-1) range required by RFC 9535 §2.1 for indices and slice bounds. Values between 2^53 and 2^63-1 were silently accepted, and values above 2^63-1 produced misleading error messages that pointed at the wrong part of the expression rather than the out-of-range integer.

The parser now rejects integers outside ±(2^53-1) in indices, slice bounds, and filter singular-query indices with a clear diagnostic. Integer literals in filter comparisons are unchanged — they are compared against JSON data values, not used for path processing, so only I64 overflow is rejected there.

## Fix Format AlignCenter with a multibyte fill character dropping right-hand padding

`Format.apply` with `AlignCenter` and a fill character that takes more than one byte in UTF-8 produced too little padding. The left fill was correct, but the right fill was missing or short. Padding is now symmetric.

## Fix Random.int and Random.int_unbiased for U128 bounds above 2^64

`Random.int[U128]` and `Random.int_unbiased[U128]` produced wrong results when the bound exceeded `U64.max_value()`. `int` never returned values at or above 2^64. `int_unbiased` confined results to the low 64 bits of the bound, so a bound of 2^64 always returned 0 and a bound of 2^64 + 5 only returned values in [0, 5).

Both methods now return correct results for U128 bounds that exceed U64 range.

## Fix http_client sending Content-Length alongside a caller-set Transfer-Encoding

When a request had a body and the caller set `Transfer-Encoding`, the library still added a `Content-Length` header. RFC 9112 section 6.2 forbids sending both. The automatic `Content-Length` is now suppressed when `Transfer-Encoding` is already present.

## Fix `Path.split` and `Path.dir` dropping the root from absolute paths

`Path.split("/a")` returned `(".", "a")` instead of `("/", "a")`, and `Path.dir("/a")` returned `"."` instead of `"/"`. The root separator was lost from any path whose last component sat directly under the root — `/a`, `/`, and the volume root of `//host/share` paths. `Path.split("//h/s")` returned `("/h", "s")` instead of `("//h/s", "")`.

## Fix `ParseURIAuthority` accepting `:::` before an embedded IPv4 address

`ParseURIAuthority`, `ParseURI`, and `URIBuilder.build` accepted IPv6 literals where `:::` preceded an embedded IPv4 address — `[:::1.2.3.4]` and `[1:::1.2.3.4]`. These are invalid: `:::` never appears in an IPv6 address. The same literals without the embedded IPv4 address (`[:::1]`, `[1:::2]`) were already rejected.

## Fix File/CreateFile callers leaving stale tails when writing shorter content

Several callers opened existing files with `File(path)` or `CreateFile(path)` and wrote from position 0 without truncating first. When new content was shorter than old content, the tail of the old data remained in the file. Affected callers: readline history saving, the pony_test regression database, pony-doc output files, and the mandelbrot benchmark.

Each caller now truncates the file with `set_length(0)` before writing.

## Fix Iter.nth and Iter.find to error when n is 0

`Iter.nth` and `Iter.find` count positions from 1. Calling `nth(0)` returned the first element instead of raising an error, making it behave identically to `nth(1)`. Calling `find(predicate, 0)` wrapped its internal counter to `USize.max_value()`, exhausted the iterator, and then raised.

Both methods now error immediately when `n` is 0.

## Remove unused `separator` parameter from `Path.split`

`Path.split` accepted a `separator` parameter that the implementation ignored — the function always used the platform's path separator regardless of the value passed. The parameter has been removed.

Code that passed a separator explicitly no longer compiles:

```pony
// Before
(let head, let tail) = Path.split(my_path, "/")

// After
(let head, let tail) = Path.split(my_path)
```

## Fix `Iter.intersperse` looping forever when `n` is 0

`Iter.intersperse(value, 0)` looped forever.

`intersperse` now treats `n == 0` as `n == 1`, matching `step_by`.

## Fix `Pbkdf2Sha256` raising on LibreSSL when the password is an empty `Array[U8]`

`Pbkdf2Sha256` raised an error on LibreSSL when the password was an empty `Array[U8]`. An empty password is valid in PBKDF2, and the same call with `""` as the password returned the correct key. On OpenSSL the empty `Array[U8]` already worked. The derivation now succeeds on both backends regardless of which empty type is used.

## Fix FrameDecoder looping forever on sub-byte frames and misplacing non-byte-aligned frames

`FrameDecoder` advanced its position in whole bytes. A codec whose frame size was not a multiple of 8 bits triggered one of two failures: frames of 1 to 7 bits caused an infinite loop because the byte offset never moved, and longer frames like 12 bits lost the sub-byte remainder on each advance, decoding every frame after the first from the wrong position.

`FrameDecoder` now tracks its position in bits and carries a sub-byte offset across `received()` calls.

## Fix http_client treating partial Transfer-Encoding matches as chunked

The http_client response parser matched "chunked" as a substring of the Transfer-Encoding header value. A value like `notchunked` or `chunked, gzip` was incorrectly parsed as chunked transfer encoding. The parser now correctly identifies `chunked` only when it appears as the final transfer coding, per RFC 9112 §6.3.

HTTP/1.0 responses with a Transfer-Encoding header are now handled as close-delimited, per RFC 9112 §6.1. Previously, `Transfer-Encoding: chunked` on an HTTP/1.0 response would attempt chunked decoding.

When Transfer-Encoding is present but the final coding is not `chunked`, Content-Length is now correctly ignored, per RFC 9112 §6.3 item 3.

## Fix http_client request builder appending a second `?` when the path already has a query string

When the path passed to `Request.get` (or any other request method) already contained a query string, calling `.query()` produced a malformed path with two `?` markers. For example, `Request.get("/s?x=1").query("q", "v")` built the path `/s?x=1?q=v` instead of `/s?x=1&q=v`. Builder parameters are now appended with `&` when the path already contains a query string.

## Fix Path.clean stripping trailing separator from volume roots

`Path.clean` stripped the trailing separator from Windows drive roots and network share roots. `clean("C:/")` produced `"C:"` and `clean("//host/share/")` produced `"//host/share"`, even though both are root directories whose trailing separator should be preserved.

`Path.split`, `Path.dir`, and `Path.join` all call `clean` internally, so the same stripping affected round-trips through those functions. `Path.join(Path.split("C:/a"))` produced `"C:a"` instead of `"C:/a"` because `split` returned `("C:", "a")` as the head.

Root directories now keep their trailing separator, matching the documented behavior: "The result will have no trailing slash unless it is a root directory."

## Fix `ParseFormURLEncoded` producing spurious pairs from empty `&`-separated sequences

`ParseFormURLEncoded` turned every `&`-delimited slice into a key-value pair, including empty slices. A leading, trailing, or doubled `&` produced a spurious `("", "")` pair: `"a=1&"` parsed to two pairs instead of one, and `"&&"` parsed to three empty pairs instead of zero. `URI.query_params()` had the same problem because it calls `ParseFormURLEncoded` directly.

Empty sequences are now skipped, matching the WHATWG URL Standard §5.1 step 3.1. A bare `=` still produces `("", "")` because it is not an empty sequence.

