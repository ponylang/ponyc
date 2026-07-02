# Known Couplings

Each file here documents one *coupling* — a non-obvious dependency where changing one part of the codebase silently breaks another, and the breakage isn't caught by tests local to the part you changed.

One coupling per file. Before a cross-cutting change, find the ones that bear on it — scan the filenames and `grep` the directory for the file or symbol you're about to change. There is deliberately no file that enumerates the others — a shared index would reintroduce the merge conflicts this directory exists to avoid (which is why these live one-per-file instead of in a single list).

See the "Known Couplings" section of the top-level `AGENTS.md` for how to read these before a change and how to record a new one.
