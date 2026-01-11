use ".."
use "pony_test"

primitive JSONBuilderTests is TestList
  fun tag tests(test: PonyTest) =>
    test(ObjBuilderTest)
    test(ArrBuilderTest)


class \nodoc\ iso ObjBuilderTest is UnitTest
  fun name(): String => "builder/obj"
  fun apply(h: TestHelper) =>
    let obj = Obj("foo", None)("bar", I64(42))("baz", F64(0.12345))("booze", false)("gosh", "lol")("snot", Arr(I64(1))(I64(2))(None)("string"))("narf", Obj("nested", "blubb")).build()
    h.assert_eq[String](
      """
      {"baz":0.12345,"snot":[1,2,null,"string"],"narf":{"nested":"blubb"},"foo":null,"booze":false,"bar":42,"gosh":"lol"}""",
      obj.string()
    )

class \nodoc\ iso ArrBuilderTest is UnitTest
  fun name(): String => "builder/arr"
  fun apply(h: TestHelper) =>
    // testing the reassign pattern
    var builder = Arr.create()
    builder = builder(Obj("foo", "bar"))(None)(I64(42))
    let json = builder.build()

    h.assert_eq[String](
      """
      [{"foo":"bar"},null,42]""",
      json.string()
    )

