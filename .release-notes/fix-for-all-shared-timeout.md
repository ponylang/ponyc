## Fix multiple for_all calls sharing first property's timeout

When a `UnitTest` called `for_all` more than once and each property called `long_test()`, only the first property's timeout took effect. The second and subsequent properties reused the first property's timer because `_TestRunner` did not reset its long-test state between queued properties.

Each `for_all` property now gets its own independent timeout when it calls `long_test()`.
