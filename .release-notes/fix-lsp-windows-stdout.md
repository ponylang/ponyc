## Fix pony-lsp hanging on startup on Windows

pony-lsp was unresponsive on Windows when launched by an editor. The LSP base protocol uses explicit `\r\n` sequences in message headers, but Windows opens stdout in text mode by default, which translates every `\n` to `\r\n`. This turned the header separator `\r\n\r\n` into `\r\r\n\r\r\n` on the wire — a sequence that LSP clients don't recognize, causing them to wait forever for the end of the headers.

pony-lsp now sets stdout to binary mode on Windows at startup, so `\r\n` is written to the pipe unchanged.
