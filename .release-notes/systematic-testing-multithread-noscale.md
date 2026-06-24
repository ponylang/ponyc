## Make systematic testing reproducible with multiple scheduler threads when scheduler scaling is disabled

Systematic testing (`use=systematic_testing`) lets you replay a runtime interleaving from a seed: run until an intermittent bug shows up, then re-run with `--ponysystematictestingseed <seed>` to replay the same interleaving and chase it down. Previously this only held with a single scheduler thread (`--ponymaxthreads 1`), and a single thread explores no interleavings at all, so reproducibility and interleaving exploration were mutually exclusive.

With scheduler scaling disabled (`--ponynoscale`), a fixed seed now replays the same interleaving across runs with more than one scheduler thread, while different seeds still produce different interleavings. The cycle detector can stay enabled. Making dynamic scheduler scaling itself reproducible under systematic testing is not yet covered; use `--ponynoscale` for reproducible multi-threaded replay until then.
