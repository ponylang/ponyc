use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    test.property(_ListReverseProperty)
    test.property(_ListReverseOneProperty)
    test(_ListReverseMultipleProperties)
    test.property(_CustomClassFlatMapProperty)
    test.property(_CustomClassMapProperty)
    test.property(_CustomClassCustomGeneratorProperty)
    test.property(_AsyncTCPSenderProperty)
    test.property(_OperationOnCollectionProperty)
    test.property(_HealthCheckProperty)
    test.property(_MultiFailureProperty)
