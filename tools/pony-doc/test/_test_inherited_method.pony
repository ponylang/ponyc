use "pony_test"
use doc = ".."

class \nodoc\ _TestInheritedMethodTypeParamNames is UnitTest
  """
  A generic method inherited from a trait is documented with the type
  parameter names written in the trait.
  """
  fun name(): String => "InheritedMethod/type-param-names"

  fun apply(h: TestHelper) =>
    let source =
      "trait Mapper\n" +
      "  fun map[R: Any val](r: R): R => r\n" +
      "class Holder[R: Any val] is Mapper\n"
    try
      let program = _DefaultValueTestHelper._compile(h, source)?
      let docs = doc.Extractor(program, false)
      let holder = _find_entity(docs, "Holder")?
      let map = _find_function(holder, "map")?
      h.assert_eq[USize](1, map.type_params.size())
      h.assert_eq[String]("R", map.type_params(0)?.name)
      match map.params(0)?.param_type
      | let r: doc.DocTypeParamRef => h.assert_eq[String]("R", r.name)
      else
        h.fail("parameter type is not a type parameter reference")
      end
    else
      h.fail("extraction failed")
    end

  fun _find_entity(docs: doc.DocProgram, entity: String): doc.DocEntity ? =>
    for m in docs.modules.values() do
      for e in m.public_types.values() do
        if e.name == entity then return e end
      end
    end
    error

  fun _find_function(entity: doc.DocEntity, method: String): doc.DocMethod ? =>
    for f in entity.public_functions.values() do
      if f.name == method then return f end
    end
    error
