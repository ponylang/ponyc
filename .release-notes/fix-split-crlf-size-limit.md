## Fix http_client rejecting at-limit status lines and chunk sizes on split reads

The `http_client` parser rejected a status line or chunk-size line that was exactly as long as its configured limit when the terminating `\r\n` arrived in a separate read. The incomplete-data size check counted a trailing `\r` as content, making a line that fit the limit appear one byte too long. The same line parsed successfully when the full CRLF arrived in the same read. The parser no longer counts a possible trailing `\r` as content in these checks.

