## Add pony-doc tool

We've added an experimental new tool, `pony-doc`, that is intended to replace the `--docs` pass in ponyc. The docs pass will remain in ponyc for now but will eventually be removed.

The most notable difference from the existing `--docs` flag is that `pony-doc` generates documentation for public items only by default. To include private types and methods in the output, use the `--include-private` flag. This replaces the old `--docs-public` flag which worked in reverse — generating everything by default and requiring a flag to restrict to public items only.

Usage:

```bash
pony-doc [options] <package-directory>
```

Options:

- `-o`, `--output`: Output directory (default: current directory)
- `--include-private`: Include private types and methods
- `-V`, `--version`: Print version and exit
