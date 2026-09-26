use "pony_test"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    test(PropertyTest[Array[USize]](_ListReverseProperty))
    test(PropertyTest[Array[USize]](_ListReverseOneProperty))
    test(_ListReverseMultipleProperties)
    test(PropertyTest[MyLittlePony](_CustomClassFlatMapProperty))
    test(PropertyTest[MyLittlePony](_CustomClassMapProperty))
    test(PropertyTest[MyLittlePony](_CustomClassCustomGeneratorProperty))
    test(PropertyTest[String](_AsyncTCPSenderProperty))
    test(
      PropertyTest[
        (USize, Array[_OperationOnCollection[String]])](
        _OperationOnCollectionProperty))
    test(PropertyTest[U8](_HealthCheckProperty))
