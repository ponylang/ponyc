## Fix pony-lsp hanging after shutdown and exit

pony-lsp would hang indefinitely after receiving the LSP `shutdown` request followed by the `exit` notification. The process had to be killed manually. The exit handler now properly disposes all actors, allowing the runtime to shut down cleanly.
