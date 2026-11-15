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

