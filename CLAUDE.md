# Project Instructions

## Issue 1532: clang-format for C source

**Branch**: `issue-1532`
**GitHub issue**: https://github.com/ponylang/ponyc/issues/1532

We are iterating on a `.clang-format` config to match the existing C style in `src/`. The draft config came from @tokenrove's comment on the issue.

### Files

- `.clang-format` — the config being iterated on
- `clang-format-check.sh` — script that runs clang-format against all `.c`, `.h`, `.cc` files in `src/` and reports per-file violation counts
- `clang-format-baseline.txt` — the baseline violation report from the initial draft config (29,504 violations across 263/275 files)

### Iteration workflow

1. Edit `.clang-format`
2. Run `./clang-format-check.sh > new-report.txt`
3. Compare: `diff clang-format-baseline.txt new-report.txt`
4. Look at the summary lines at the bottom for total_files, failing_files, total_violations

### Known violation categories (from initial analysis)

1. Preprocessor indentation (`#  define` inside `#ifdef`) — biggest source; `IndentPPDirectives` option could help
2. `switch`/`case` indentation — codebase indents `case` inside `switch`; `IndentCaseLabels` option
3. Short function brace placement
4. Macro continuation backslash alignment
5. Function argument wrapping
6. Include reordering — `SortIncludes` option
7. AST builder macro calls (`BUILD`/`NODE`/`TREE`) mangled — most problematic; may need `// clang-format off` blocks
8. Space before parens in `for`/`switch`/`if`

### Cleanup

When this work is complete (merged or abandoned), remove:
- This section from `CLAUDE.md` (or the entire file if nothing else is in it)
- `clang-format-check.sh`
- `clang-format-baseline.txt`
