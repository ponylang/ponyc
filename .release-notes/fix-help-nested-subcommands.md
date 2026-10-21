## Fix help for nested CLI subcommands

The `help` subcommand and `--help` flag in the `cli` package could only show help for top-level commands. Requesting help for a nested command path — `app help config server` or `app config server --help` — failed with "too many positional arguments" or "unknown command" instead of printing the subcommand's help text.

Both paths now accept multi-level command paths and display the correct help.
