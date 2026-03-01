use "pony_test"

class iso Property1UnitTest[T] is UnitTest
  """
  Provides plumbing for integration of PonyCheck
  [Properties](pony_check-Property1.md) into [PonyTest](pony_test--index.md).

  Wrap your properties into this class and use it in a
  [TestList](pony_test-TestList.md):

  ```pony
  use "pony_test"
  use "pony_check"

  class MyProperty is Property1[String]
    fun name(): String => "my_property"

    fun gen(): Generator[String] =>
      Generators.ascii_printable()

    fun property(arg1: String, h: PropertyHelper) =>
      h.assert_true(arg1.size() > 0)

  actor Main is TestList
    new create(env: Env) => PonyTest(env, this)

    fun tag tests(test: PonyTest) =>
      test(Property1UnitTest[String](MyProperty))

  ```
  """

  var _prop1: ( Property1[T] iso | None )
  let _name: String

  new iso create(p1: Property1[T] iso, name': (String | None) = None) =>
    """
    Wrap a [Property1](pony_check-Property1.md) to make it mimic the PonyTest
    [UnitTest](pony_test-UnitTest.md).

    If `name'` is given, use this as the test name.
    If not, use the property's `name()`.
    """
    _name =
      match \exhaustive\ name'
      | None => p1.name()
      | let s: String => s
      end
    _prop1 = consume p1


  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    let prop = ((_prop1 = None) as Property1[T] iso^)
    let params = prop.params()
    h.long_test(params.timeout)
    let property_runner =
      PropertyRunner[T](
        consume prop,
        params,
        h, // treat it as PropertyResultNotify
        h,  // is also a PropertyLogger for us
        h.env
      )
    h.dispose_when_done(property_runner)
    property_runner.run()

class iso Property2UnitTest[T1, T2] is UnitTest

  var _prop2: ( Property2[T1, T2] iso | None )
  let _name: String

  new iso create(p2: Property2[T1, T2] iso, name': (String | None) = None) =>
    _name =
      match \exhaustive\ name'
      | None => p2.name()
      | let s: String => s
      end
    _prop2 = consume p2

  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    let prop = ((_prop2 = None) as Property2[T1, T2] iso^)
    let params = prop.params()
    h.long_test(params.timeout)
    let property_runner =
      PropertyRunner[(T1, T2)](
        consume prop,
        params,
        h, // PropertyResultNotify
        h, // PropertyLogger
        h.env
      )
    h.dispose_when_done(property_runner)
    property_runner.run()

class iso Property3UnitTest[T1, T2, T3] is UnitTest

  var _prop3: ( Property3[T1, T2, T3] iso | None )
  let _name: String

  new iso create(p3: Property3[T1, T2, T3] iso, name': (String | None) = None) =>
    _name =
      match \exhaustive\ name'
      | None => p3.name()
      | let s: String => s
      end
    _prop3 = consume p3

  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    let prop = ((_prop3 = None) as Property3[T1, T2, T3] iso^)
    let params = prop.params()
    h.long_test(params.timeout)
    let property_runner =
      PropertyRunner[(T1, T2, T3)](
        consume prop,
        params,
        h, // PropertyResultNotify
        h, // PropertyLogger
        h.env
      )
    h.dispose_when_done(property_runner)
    property_runner.run()

class iso Property4UnitTest[T1, T2, T3, T4] is UnitTest

  var _prop4: ( Property4[T1, T2, T3, T4] iso | None )
  let _name: String

  new iso create(p4: Property4[T1, T2, T3, T4] iso, name': (String | None) = None) =>
    _name =
      match \exhaustive\ name'
      | None => p4.name()
      | let s: String => s
      end
    _prop4 = consume p4

  fun name(): String => _name

  fun ref apply(h: TestHelper) ? =>
    let prop = ((_prop4 = None) as Property4[T1, T2, T3, T4] iso^)
    let params = prop.params()
    h.long_test(params.timeout)
    let property_runner =
      PropertyRunner[(T1, T2, T3, T4)](
        consume prop,
        params,
        h, // PropertyResultNotify
        h, // PropertyLogger
        h.env
      )
    h.dispose_when_done(property_runner)
    property_runner.run()

