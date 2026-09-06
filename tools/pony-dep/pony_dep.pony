"""
pony-dep: A dependency manager for Pony packages.

Manages external dependencies by fetching, placing, and tracking package
archives. Dependencies are recorded in a `pony.deps` configuration file
with content hashes so that different versions of the same package coexist
without a solver. Use `ConfigParser` to parse a `pony.deps` file into a
`ConfigFile` containing `DepEntry` values.

**Subcommands:**

- `pack` — create an archive from a project's source for distribution.
- `fetch` — download and extract a package archive from a URL.
- `add` — fetch a dependency, compute its content hash, and record it in
  the configuration file.
- `remove` — remove a dependency's configuration entry and its placed files.
- `clean` — scan source for `use "ext:..."` references and remove any placed
  package directory that nothing references.
"""
