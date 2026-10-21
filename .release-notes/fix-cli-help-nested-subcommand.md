## Fix --help returning SyntaxError on second-level subcommands

Using `--help` on a command nested two or more levels deep (e.g., `app config server --help`) returned `SyntaxError: unknown command` instead of the expected `CommandHelp`. First-level subcommands and direct command parsing were unaffected.
