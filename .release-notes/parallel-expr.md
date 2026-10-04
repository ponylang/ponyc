## Add parallel expression type-checking

The compiler now type-checks packages in parallel. Packages whose dependencies have already been type-checked are dispatched to worker threads; mutually dependent packages are grouped and type-checked together.

The `--jobs` (`-j`) flag controls the number of worker threads. The default is auto-detect (one thread per CPU core). Pass `-j 1` for serial type-checking.
