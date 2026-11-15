## Fix JSONPath parser accepting integers outside RFC 9535 range

The JSONPath parser accepted integers outside the ±(2^53-1) range required by RFC 9535 §2.1 for indices and slice bounds. Values between 2^53 and 2^63-1 were silently accepted, and values above 2^63-1 produced misleading error messages that pointed at the wrong part of the expression rather than the out-of-range integer.

The parser now rejects integers outside ±(2^53-1) in indices, slice bounds, and filter singular-query indices with a clear diagnostic. Integer literals in filter comparisons are unchanged — they are compared against JSON data values, not used for path processing, so only I64 overflow is rejected there.
