# Known Couplings

Each file here documents one *coupling* — a dependency that crosses a file or system boundary, where changing code in one file silently breaks (or forces a matching edit in) a *different* file, and no build, test, or CI step near the change catches it. Spooky action at a distance, between two things this repo maintains that both change over time. Couplings are rare; a loud failure (a compile error, a red CI step) is caught the moment you build, so it is not a coupling.

One coupling per file. Before a cross-cutting change, find the ones that bear on it — scan the filenames and `grep` the directory for the file or symbol you're about to change. There is deliberately no file that enumerates the others — a shared index would reintroduce the merge conflicts this directory exists to avoid (which is why these live one-per-file instead of in a single list).

See the "Known Couplings" section of the top-level `AGENTS.md` for how to read these before a change and how to record a new one.
