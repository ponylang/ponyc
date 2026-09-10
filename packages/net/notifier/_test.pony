use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(_TestNotifierPingPong)
    test(_TestNotifierSSLPingPong)
    test(_TestNotifierConnectFailed)
    test(_TestNotifierBufferUntil)
    test(_TestNotifierDispose)
    test(_TestNotifierRejectConnection)
    test(_TestNotifierUDPEcho)
    test(_TestNotifierUDPDispose)
