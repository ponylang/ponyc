## Time the reach and paint passes with --pass-timings

The `--pass-timings` table and the `--pass-timings-json` file now include the reach and paint passes alongside the front-end passes. Reach and paint run over the whole program rather than a single package, so their rows show `<program>` in the package column.

## Speed up the reachability pass on large programs

The reachability pass now runs about three times faster on programs with many reachable types.

