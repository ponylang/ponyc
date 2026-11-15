## Fix http_client accepting malformed status codes with extra characters

The `http_client` response parser accepted status lines where extra characters followed the three-digit status code. `HTTP/1.1 2000 OK` silently parsed as status 200 with reason phrase `0 OK`. The parser now rejects any status line where the byte after the three digits is not a space or the end of the line.
