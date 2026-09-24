## Add regression persistence to PonyCheck

When a property test fails, the shrunk failing choice sequence is saved to disk. On the next run, the stored sequence is replayed before random samples are generated. If it still fails, the property fails immediately with the same minimal case. If it passes, the stored regression is cleared and all configured random samples run normally.

Both `Property1` and `StatefulProperty` support regression persistence. Regressions are stored in a `.ponycheck/` directory under the working directory, one file per property.

Two environment variables control the behavior:

- `PONYCHECK_NO_DB=1` disables persistence entirely.
- `PONYCHECK_DB_DIR=path` changes the storage directory.

Individual properties can opt out via `PropertyParams`:

```pony
fun params(): PropertyParams =>
  PropertyParams(where regression_db' = false)
```

Persistence is off for properties registered through the `ForAll` convenience API.
