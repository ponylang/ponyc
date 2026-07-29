// The runtime rejects a memory profile outside 1 to 10 set through
// RuntimeOptions; 11 aborts at startup before Main runs, so the runtime's
// error exit code is what the test checks.
actor Main
  new create(env: Env) => None

  fun @runtime_override_defaults(rto: RuntimeOptions) =>
    rto.ponymemoryprofile = 11
