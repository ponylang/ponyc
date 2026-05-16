## Reject adjacent string literals

The compiler used to silently accept code where two string literals were placed back-to-back with no separator between them, treating them as two unrelated expressions. This most commonly came up as a confusing failure mode for typos involving `"""`, where a missing or misplaced quote produced a program that compiled but behaved nothing like what was written. Adjacent string literals are now reported as a syntax error.
