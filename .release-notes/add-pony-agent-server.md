## Add pony-agent-server

A new experimental tool for AI coding agents that need Pony type information while editing code. It compiles a package and answers queries about types, definitions, capabilities, and structure. The query interface is experimental and subject to change.

Start the server on a package directory:

```bash
pony-agent-server /path/to/my-project
```

Supported queries: `inspect`, `scope`, `exports`, `type_api`, `check`, `implementors`, `errors`, `reload`, and `status`. The server reads one JSON query per line from stdin and writes one JSON response per line to stdout.
