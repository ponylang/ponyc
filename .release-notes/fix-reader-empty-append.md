## Fix Reader crash when appending empty data

Calling `Reader.append` with an empty `Array[U8]` or empty `String` pushed a zero-length chunk into the internal chunk list. A subsequent read — even with sufficient data from other appends — would error when trying to index into the empty chunk.
