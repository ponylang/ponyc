use "pony_test"
use doc = ".."

class \nodoc\ _TestEntityKindStrings is UnitTest
  fun name(): String => "EntityKind/strings"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](doc.EntityClass.string(), "class")
    h.assert_eq[String](doc.EntityActor.string(), "actor")
    h.assert_eq[String](doc.EntityTrait.string(), "trait")
    h.assert_eq[String](doc.EntityInterface.string(), "interface")
    h.assert_eq[String](doc.EntityPrimitive.string(), "primitive")
    h.assert_eq[String](doc.EntityStruct.string(), "struct")
    h.assert_eq[String](doc.EntityTypeAlias.string(), "type")

    // keyword() should match string() for entity kinds
    h.assert_eq[String](doc.EntityClass.keyword(), "class")
    h.assert_eq[String](doc.EntityActor.keyword(), "actor")
    h.assert_eq[String](doc.EntityTrait.keyword(), "trait")
    h.assert_eq[String](doc.EntityInterface.keyword(), "interface")
    h.assert_eq[String](doc.EntityPrimitive.keyword(), "primitive")
    h.assert_eq[String](doc.EntityStruct.keyword(), "struct")
    h.assert_eq[String](doc.EntityTypeAlias.keyword(), "type")

class \nodoc\ _TestMethodKindStrings is UnitTest
  fun name(): String => "MethodKind/strings"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](doc.MethodConstructor.string(), "new")
    h.assert_eq[String](doc.MethodFunction.string(), "fun")
    h.assert_eq[String](doc.MethodBehaviour.string(), "be")

class \nodoc\ _TestFieldKindStrings is UnitTest
  fun name(): String => "FieldKind/strings"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](doc.FieldVar.string(), "var")
    h.assert_eq[String](doc.FieldLet.string(), "let")
    h.assert_eq[String](doc.FieldEmbed.string(), "embed")
