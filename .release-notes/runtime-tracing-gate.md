## Preserve runtime tracing across thread startup and category changes

Runtime-enabled tracing could stop recording events when a runtime thread started or when category changes reached schedulers at different times. Tracing now remains enabled or disabled according to the latest category change.
