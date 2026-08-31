## Fix PropertyRunner double completion on assert-and-error

In rare cases where a property test failed an assertion and raised an error, the runner sent two completion notifications instead of one, causing the test harness to fall out of sync. The completion notification is now sent exactly once.
