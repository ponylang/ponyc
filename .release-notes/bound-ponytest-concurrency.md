## Bound PonyTest concurrent execution to scheduler thread count

PonyTest previously launched all non-exclusive tests at once with no concurrency limit. In large test suites this overwhelmed the scheduler, causing spurious timeouts in tests that pass when run on their own.

Concurrent test execution is now bounded by the number of scheduler threads. Tests beyond that limit are queued and started as earlier tests complete. Small test suites are unaffected — when there are fewer tests than threads, all tests still start immediately.
