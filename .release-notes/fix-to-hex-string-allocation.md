## Fix unnecessary per-byte allocation in ToHexString

`ToHexString` allocated a temporary string for every input byte. Converting a SHA-512 hash to hex produced 64 intermediate strings. The conversion now runs with a single allocation for the output.
