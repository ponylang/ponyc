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

