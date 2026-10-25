# benchmarks/micro/

## Conventions

- Do not add coupling comments in `src/libponyrt/` pointing back to
  `benchmarks/micro/`. Benchmarks depend on runtime constants — the coupling
  risk is theirs, and the note goes on the benchmark side alone.
