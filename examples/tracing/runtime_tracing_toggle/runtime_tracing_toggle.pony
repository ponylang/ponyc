"""
Toggles runtime tracing categories at runtime. Enables scheduler and actor
behavior tracing, does some work that generates trace events, narrows the
active set, then clears all categories.

Run with `--ponytracingoutput=trace.json` to see the output.
"""
