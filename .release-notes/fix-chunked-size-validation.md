## Fix http_client accepting underscores and empty chunk sizes in chunked responses

The `http_client` chunked transfer-encoding parser accepted chunk sizes that contain underscores (`1_0` parsed as 16) and empty chunk sizes before extensions (`;ext` parsed as 0, ending the body). RFC 9112 §7.1 requires `1*HEXDIG` — at least one hex digit and nothing else. The parser now rejects any chunk size that contains a non-hex character or is empty.

The parser also now accepts optional whitespace (SP, HTAB) between the chunk size and the semicolon of a chunk extension, as RFC 9112 §7.1.1 allows.
