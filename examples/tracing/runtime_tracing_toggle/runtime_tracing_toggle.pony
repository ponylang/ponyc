"""
Toggles runtime tracing categories and per-actor tracing at runtime. The
program enables scheduler and actor tracing, does some work that generates
trace events, disables actor tracing, and then clears all categories.

Run with `--ponytracingformat=json --ponytracingoutput=trace.json` to see the
output.
"""
