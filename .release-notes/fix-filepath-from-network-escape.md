## Fix `FilePath.from` letting `..` escape a `//` base path

`FilePath.from` accepted `..` paths that escaped the base directory when the base path started with exactly `//` and had one or two components after it (e.g., `//srv/www`). On Linux, where `//` resolves to `/`, the returned `FilePath` carried the base's capabilities but pointed outside the base. Every `Directory` method that takes a relative path had the same escape.

Paths starting with `//` followed by fewer than two valid components (e.g., `//host/` with a trailing slash and no share name) were also accepted as network volumes. They are now rejected, and the leading `//` collapses to `/` during cleaning.

`FilePath.from` now rejects any cleaned path that still contains a `..` component.
