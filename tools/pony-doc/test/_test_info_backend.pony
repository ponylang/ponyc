use "files"
use "pony_test"
use doc = ".."

class \nodoc\ _TestInfoLinkFormatLink is UnitTest
  fun name(): String => "InfoLinkFormat/link"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](
      doc.InfoLinkFormat.link("Array", "builtin-Array"),
      "*note Array: builtin-Array.")

class \nodoc\ _TestInfoLinkFormatBrackets is UnitTest
  fun name(): String => "InfoLinkFormat/brackets"

  fun apply(h: TestHelper) =>
    h.assert_eq[String](doc.InfoLinkFormat.open_bracket(), "[")
    h.assert_eq[String](doc.InfoLinkFormat.close_bracket(), "]")

class \nodoc\ _TestInfoBackendMinimal is UnitTest
  fun name(): String => "InfoBackend/minimal"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h, "minimal"))
    tmp.mkdir()

    let entity = _InfoTestHelper.entity(
      "MyClass", "testpkg-MyClass", doc.EntityClass
      where methods = recover val
        [as doc.DocMethod:
          doc.DocMethod(
            doc.MethodFunction,
            "size",
            "box",
            recover val Array[doc.DocTypeParam] end,
            recover val Array[doc.DocParam] end,
            doc.DocNominal(
              "USize", "builtin-USize",
              recover val Array[doc.DocType] end,
              None, None, false, false),
            false,
            "Returns the size.",
            None)]
      end)
    let program = _InfoTestHelper.program("testpkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp, "testpkg.info")?

    // Preamble
    h.assert_true(
      content.contains("This is testpkg.info, produced by pony-doc."),
      "preamble missing")

    // Top node
    h.assert_true(
      content.contains("Node: Top"),
      "Top node missing")

    // Package node
    h.assert_true(
      content.contains("Node: testpkg--index"),
      "package node missing")

    // Entity node
    h.assert_true(
      content.contains("Node: testpkg-MyClass"),
      "entity node missing")

    // Tag table
    h.assert_true(
      content.contains("Tag Table:"),
      "tag table missing")
    h.assert_true(
      content.contains("End Tag Table"),
      "end tag table missing")
    h.assert_true(
      content.contains("Node: Top\x7F"),
      "Top tag entry missing")

    // Method rendered in entity
    h.assert_true(
      content.contains("-- fun box size"),
      "method missing from entity")
    h.assert_true(
      content.contains("Returns the size."),
      "method docstring missing")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendNavigation is UnitTest
  fun name(): String => "InfoBackend/navigation"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"navigation"))
    tmp.mkdir()

    let entity1 = _InfoTestHelper.entity("Foo", "mypkg-Foo", doc.EntityClass)
    let entity2 = _InfoTestHelper.entity("Bar", "mypkg-Bar", doc.EntityPrimitive)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity1; entity2] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    // Top: Next=mypkg--index, no Prev, Up=(dir)
    h.assert_true(
      content.contains(
        "Node: Top,  Next: mypkg--index,  Up: (dir)"),
      "Top navigation wrong")

    // Package: Next=first entity, Prev=Top, Up=Top
    h.assert_true(
      content.contains(
        "Node: mypkg--index,  Next: mypkg-Foo,  Prev: Top,  Up: Top"),
      "package navigation wrong")

    // First entity: Next=second entity, Prev=package, Up=package
    h.assert_true(
      content.contains(
        "Node: mypkg-Foo,  Next: mypkg-Bar,"
          + "  Prev: mypkg--index,  Up: mypkg--index"),
      "first entity navigation wrong")

    // Second entity: Next=Type Index, Prev=first entity, Up=package
    h.assert_true(
      content.contains(
        "Node: mypkg-Bar,  Next: Type Index,"
          + "  Prev: mypkg-Foo,  Up: mypkg--index"),
      "second entity navigation wrong")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendMenuStructure is UnitTest
  fun name(): String => "InfoBackend/menu-structure"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"menu"))
    tmp.mkdir()

    let entity = _InfoTestHelper.entity("Foo", "mypkg-Foo", doc.EntityClass)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("* mypkg--index::"),
      "Top menu missing package entry")

    h.assert_true(
      content.contains("* mypkg-Foo::    class Foo"),
      "package menu missing entity entry")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendCrossReferences is UnitTest
  fun name(): String => "InfoBackend/cross-references"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"xref"))
    tmp.mkdir()

    let provides_type = doc.DocNominal(
      "Stringable", "builtin-Stringable",
      recover val Array[doc.DocType] end,
      None, None, false, false)

    let entity = doc.DocEntity(
      doc.EntityClass,
      "Foo",
      "mypkg-Foo",
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val [as doc.DocType: provides_type] end,
      None,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocField] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      None)

    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    // Entity node contains *note cross-reference for provides type
    h.assert_true(
      content.contains("*note Stringable: builtin-Stringable."),
      "cross-reference missing from entity node")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendPrivateExcluded is UnitTest
  fun name(): String => "InfoBackend/private-excluded-from-menu"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"priv-excl"))
    tmp.mkdir()

    let public_entity = _InfoTestHelper.entity("Foo", "mypkg-Foo", doc.EntityClass)
    let private_entity = _InfoTestHelper.entity("_Bar", "mypkg-_Bar", doc.EntityClass)

    let mod = doc.DocModule(
      "mypkg",
      None,
      recover val [as doc.DocEntity: public_entity] end,
      recover val [as doc.DocEntity: private_entity] end,
      recover val Array[doc.DocSourceFile] end)
    let program = doc.DocProgram(
      "mypkg",
      recover val [as doc.DocModule: mod] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    // Private entity node still exists (for navigation/tag table)
    h.assert_true(
      content.contains("Node: mypkg-_Bar"),
      "private entity node missing")

    h.assert_false(
      content.contains("* mypkg-_Bar::"),
      "private entity should not be in menu with include_private=false")

    h.assert_true(
      content.contains("* mypkg-Foo::"),
      "public entity missing from menu")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendPrivateIncluded is UnitTest
  fun name(): String => "InfoBackend/private-included-in-menu"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"priv-incl"))
    tmp.mkdir()

    let public_entity = _InfoTestHelper.entity("Foo", "mypkg-Foo", doc.EntityClass)
    let private_entity = _InfoTestHelper.entity("_Bar", "mypkg-_Bar", doc.EntityClass)

    let mod = doc.DocModule(
      "mypkg",
      None,
      recover val [as doc.DocEntity: public_entity] end,
      recover val [as doc.DocEntity: private_entity] end,
      recover val Array[doc.DocSourceFile] end)
    let program = doc.DocProgram(
      "mypkg",
      recover val [as doc.DocModule: mod] end)

    doc.InfoBackend.generate(program, tmp, true)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("* mypkg-_Bar::"),
      "private entity should be in menu with include_private=true")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendTypeAlias is UnitTest
  fun name(): String => "InfoBackend/type-alias"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"alias"))
    tmp.mkdir()

    let aliased_type = doc.DocNominal(
      "U32", "builtin-U32",
      recover val Array[doc.DocType] end,
      None, None, false, false)

    let entity = doc.DocEntity(
      doc.EntityTypeAlias,
      "MyAlias",
      "mypkg-MyAlias",
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val [as doc.DocType: aliased_type] end,
      None,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocField] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      None)

    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("Type Alias For"),
      "'Type Alias For' heading missing")
    h.assert_false(
      content.contains("Implements"),
      "type alias should not have 'Implements' heading")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendEmptyModule is UnitTest
  fun name(): String => "InfoBackend/empty-module"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"empty"))
    tmp.mkdir()

    let program = _InfoTestHelper.program("emptypkg",
      recover val Array[doc.DocEntity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"emptypkg.info")?

    // Package node exists
    h.assert_true(
      content.contains("Node: emptypkg--index"),
      "package node missing for empty module")

    // No "* Menu:" inside the package node content (only in Top)
    let pkg_header = "Node: emptypkg--index,"
    let pkg_start =
      try content.find(pkg_header)? else h.fail("can't find pkg"); 0 end
    let next_separator =
      try content.find("\x1F" where offset = pkg_start + 1)?
      else content.size()
      end
    let pkg_section = content.substring(
      pkg_start.isize(), next_separator.isize())

    h.assert_false(
      pkg_section.contains("* Menu:"),
      "empty module should have no menu")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendTypeIndex is UnitTest
  fun name(): String => "InfoBackend/type-index"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"type-idx"))
    tmp.mkdir()

    let entity1 = _InfoTestHelper.entity("Foo", "mypkg-Foo", doc.EntityClass)
    let entity2 = _InfoTestHelper.entity("Bar", "mypkg-Bar", doc.EntityPrimitive)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity1; entity2] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("Node: Type Index"),
      "Type Index node missing")
    h.assert_true(
      content.contains("* mypkg-Foo:"),
      "Foo missing from type index")
    h.assert_true(
      content.contains("* mypkg-Bar:"),
      "Bar missing from type index")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendTypeIndexPrivate is UnitTest
  fun name(): String => "InfoBackend/type-index-private"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth,
      _InfoTestHelper.tmp_dir(h,"type-idx-priv"))
    tmp.mkdir()

    let pub_entity =
      _InfoTestHelper.entity("Foo", "mypkg-Foo", doc.EntityClass)
    let priv_entity =
      _InfoTestHelper.entity("_Bar", "mypkg-_Bar", doc.EntityPrimitive)

    let mod = doc.DocModule(
      "mypkg",
      None,
      recover val [as doc.DocEntity: pub_entity] end,
      recover val [as doc.DocEntity: priv_entity] end,
      recover val Array[doc.DocSourceFile] end)
    let program = doc.DocProgram(
      "mypkg",
      recover val [as doc.DocModule: mod] end)

    doc.InfoBackend.generate(program, tmp, true)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    // Extract the Type Index section
    let ti_start =
      try content.find("Node: Type Index")?
      else h.fail("Type Index node missing"); 0
      end
    let ti_end =
      try content.find("\x1F" where offset = ti_start + 1)?
      else content.size()
      end
    let type_idx = content.substring(ti_start.isize(), ti_end.isize())

    h.assert_true(
      type_idx.contains("* mypkg-Foo:"),
      "public entity missing from type index")
    h.assert_true(
      type_idx.contains("* mypkg-_Bar:"),
      "private entity should be in type index with include_private=true")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendMethodIndex is UnitTest
  fun name(): String => "InfoBackend/method-index"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"meth-idx"))
    tmp.mkdir()

    let entity = _InfoTestHelper.entity("MyClass", "mypkg-MyClass", doc.EntityClass
      where methods = recover val
        [as doc.DocMethod:
          doc.DocMethod(
            doc.MethodFunction,
            "size",
            "box",
            recover val Array[doc.DocTypeParam] end,
            recover val Array[doc.DocParam] end,
            None,
            false,
            None,
            None)]
      end)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("Node: Method Index"),
      "Method Index node missing")
    h.assert_true(
      content.contains("* MyClass.size:"),
      "MyClass.size missing from method index")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendTagTableOffsets is UnitTest
  fun name(): String => "InfoBackend/tag-table-offsets"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"offsets"))
    tmp.mkdir()

    let entity = _InfoTestHelper.entity("Foo", "mypkg-Foo", doc.EntityClass)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    // Extract a tag table entry and verify the offset points to \x1F
    let tag_start =
      try content.find("Tag Table:")? else h.fail("no tag table"); 0 end
    let top_entry_start =
      try content.find("Node: Top\x7F" where offset = tag_start)?
      else h.fail("no Top entry in tag table"); 0
      end

    // Parse the offset after \x7F
    let del_pos =
      try content.find("\x7F" where offset = top_entry_start)?
      else h.fail("no \\x7F"); 0
      end
    let nl_pos =
      try content.find("\n" where offset = del_pos)?
      else content.size()
      end
    let offset_str = content.substring(
      (del_pos + 1).isize(), nl_pos.isize())
    let offset =
      try offset_str.usize()? else h.fail("bad offset"); 0 end

    // The byte at the recorded offset should be \x1F
    try
      h.assert_eq[U8](content(offset)?, 0x1F,
        "offset does not point to node separator")
    else
      h.fail("offset out of range")
    end

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendFields is UnitTest
  fun name(): String => "InfoBackend/fields"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"fields"))
    tmp.mkdir()

    let field_type = doc.DocNominal(
      "String", "builtin-String",
      recover val Array[doc.DocType] end,
      "val", None, false, false)

    let entity = doc.DocEntity(
      doc.EntityClass,
      "MyClass",
      "mypkg-MyClass",
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val Array[doc.DocType] end,
      None,
      recover val Array[doc.DocMethod] end,
      recover val [as doc.DocField:
        doc.DocField(
          doc.FieldLet,
          "name",
          field_type,
          "The name.",
          None)]
      end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      None)

    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("Public fields"),
      "'Public fields' section missing")
    h.assert_true(
      content.contains("-- let name: "),
      "field declaration missing")
    h.assert_true(
      content.contains("The name."),
      "field docstring missing")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendMultiplePackages is UnitTest
  fun name(): String => "InfoBackend/multiple-packages"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"multi-pkg"))
    tmp.mkdir()

    let entity1 = _InfoTestHelper.entity("Foo", "pkg1-Foo", doc.EntityClass)
    let entity2 = _InfoTestHelper.entity("Bar", "pkg2-Bar", doc.EntityPrimitive)

    let mod1 = doc.DocModule(
      "pkg1",
      "Package one.",
      recover val [as doc.DocEntity: entity1] end,
      recover val Array[doc.DocEntity] end,
      recover val Array[doc.DocSourceFile] end)
    let mod2 = doc.DocModule(
      "pkg2",
      "Package two.",
      recover val [as doc.DocEntity: entity2] end,
      recover val Array[doc.DocEntity] end,
      recover val Array[doc.DocSourceFile] end)

    let program = doc.DocProgram(
      "multipkg",
      recover val [as doc.DocModule: mod1; mod2] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"multipkg.info")?

    h.assert_true(
      content.contains("* pkg1--index::"),
      "pkg1 missing from Top menu")
    h.assert_true(
      content.contains("* pkg2--index::"),
      "pkg2 missing from Top menu")

    h.assert_true(
      content.contains("Node: pkg1--index,"),
      "pkg1 node missing")
    h.assert_true(
      content.contains("Node: pkg2--index,"),
      "pkg2 node missing")

    // Navigation crosses package boundary
    h.assert_true(
      content.contains(
        "Node: pkg1-Foo,  Next: pkg2--index,"
          + "  Prev: pkg1--index,  Up: pkg1--index"),
      "cross-package navigation wrong")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendTagTableAllOffsets is UnitTest
  fun name(): String => "InfoBackend/tag-table-all-offsets"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"tag-all"))
    tmp.mkdir()

    let entity = _InfoTestHelper.entity(
      "Foo", "mypkg-Foo", doc.EntityClass)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    let tag_start: ISize =
      try content.find("Tag Table:")? else h.fail("no tag table"); 0 end
    let end_tag: ISize =
      try content.find("End Tag Table")?
      else h.fail("no end tag table"); content.size().isize()
      end

    var search_from: ISize = tag_start
    while search_from < end_tag do
      let entry_start: ISize =
        try content.find("Node: " where offset = search_from)?
        else break
        end
      let del: ISize =
        try content.find("\x7F" where offset = entry_start)?
        else h.fail("no \\x7F after Node:"); break
        end
      let nl: ISize =
        try content.find("\n" where offset = del)?
        else content.size().isize()
        end
      let offset_str_iso = content.substring((del + 1), nl)
      let offset_str: String val = consume offset_str_iso
      let offset: USize =
        try offset_str.usize()? else h.fail("bad offset"); break end
      try
        h.assert_eq[U8](content(offset)?, 0x1F,
          "offset " + offset_str + " does not point to \\x1F")
      else
        h.fail("offset " + offset_str + " out of range")
      end
      search_from = nl + 1
    end

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendMethodCategories is UnitTest
  fun name(): String => "InfoBackend/method-categories"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"meth-cat"))
    tmp.mkdir()

    let return_type = doc.DocNominal(
      "Foo", "mypkg-Foo",
      recover val Array[doc.DocType] end,
      None, None, false, false)

    let ctor = doc.DocMethod(
      doc.MethodConstructor,
      "create",
      None,
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      return_type,
      false,
      None,
      None)

    let behav = doc.DocMethod(
      doc.MethodBehaviour,
      "update",
      None,
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      None,
      false,
      None,
      None)

    let func = doc.DocMethod(
      doc.MethodFunction,
      "size",
      "box",
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      None,
      false,
      None,
      None)

    let entity = doc.DocEntity(
      doc.EntityActor,
      "Foo",
      "mypkg-Foo",
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val Array[doc.DocType] end,
      None,
      recover val [as doc.DocMethod: ctor] end,
      recover val Array[doc.DocField] end,
      recover val [as doc.DocMethod: behav] end,
      recover val [as doc.DocMethod: func] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      None)

    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("Constructors\n************"),
      "Constructors heading missing")
    h.assert_true(
      content.contains("-- new create"),
      "constructor missing")
    h.assert_true(
      content.contains("Public Behaviours\n*****************"),
      "Public Behaviours heading missing")
    h.assert_true(
      content.contains("-- be update"),
      "behaviour missing")
    h.assert_true(
      content.contains("Public Functions\n****************"),
      "Public Functions heading missing")
    h.assert_true(
      content.contains("-- fun box size"),
      "function missing")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendMethodParams is UnitTest
  fun name(): String => "InfoBackend/method-params"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"meth-params"))
    tmp.mkdir()

    let string_type = doc.DocNominal(
      "String", "builtin-String",
      recover val Array[doc.DocType] end,
      None, None, false, false)
    let usize_type = doc.DocNominal(
      "USize", "builtin-USize",
      recover val Array[doc.DocType] end,
      None, None, false, false)

    let method = doc.DocMethod(
      doc.MethodFunction,
      "apply",
      "box",
      recover val Array[doc.DocTypeParam] end,
      recover val [as doc.DocParam:
        doc.DocParam("key", string_type, None)
        doc.DocParam("offset", usize_type, "0")]
      end,
      string_type,
      false,
      None,
      None)

    let entity = _InfoTestHelper.entity(
      "MyMap", "mypkg-MyMap", doc.EntityClass
      where methods = recover val
        [as doc.DocMethod: method]
      end)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("key: *note String: builtin-String."),
      "first param missing")
    h.assert_true(
      content.contains("offset: *note USize: builtin-USize. = 0"),
      "param with default missing")
    h.assert_true(
      content.contains("): *note String: builtin-String."),
      "return type missing")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendPartialMethod is UnitTest
  fun name(): String => "InfoBackend/partial-method"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"partial"))
    tmp.mkdir()

    let string_type = doc.DocNominal(
      "String", "builtin-String",
      recover val Array[doc.DocType] end,
      None, None, false, false)

    let method = doc.DocMethod(
      doc.MethodFunction,
      "parse",
      "box",
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      string_type,
      true,
      None,
      None)

    let entity = _InfoTestHelper.entity(
      "Parser", "mypkg-Parser", doc.EntityClass
      where methods = recover val
        [as doc.DocMethod: method]
      end)
    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("): *note String: builtin-String. ?"),
      "partial marker missing from method signature")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendPkgNodeSlash is UnitTest
  fun name(): String => "InfoBackend/pkg-node-slash"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"slash"))
    tmp.mkdir()

    let entity = _InfoTestHelper.entity(
      "Foo", "a/b-Foo", doc.EntityClass)
    let mod = doc.DocModule(
      "a/b",
      "Slashed path.",
      recover val [as doc.DocEntity: entity] end,
      recover val Array[doc.DocEntity] end,
      recover val Array[doc.DocSourceFile] end)
    let program = doc.DocProgram(
      "slashpkg",
      recover val [as doc.DocModule: mod] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"slashpkg.info")?

    // The package node name replaces / with - and appends --index
    h.assert_true(
      content.contains("Node: a-b--index,"),
      "slash-replaced node name missing")
    h.assert_true(
      content.contains("* a-b--index::"),
      "slash-replaced menu entry missing")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendEntityDocstring is UnitTest
  fun name(): String => "InfoBackend/entity-docstring"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"entdoc"))
    tmp.mkdir()

    let entity = doc.DocEntity(
      doc.EntityClass,
      "Documented",
      "mypkg-Documented",
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val Array[doc.DocType] end,
      "A well-documented class for testing.",
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocField] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      None)

    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("A well-documented class for testing."),
      "entity docstring missing from node")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendPrivateMethodIndex is UnitTest
  """Verify private methods excluded from Method Index when not requested."""
  fun name(): String => "InfoBackend/private-method-index"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth, _InfoTestHelper.tmp_dir(h,"priv-meth"))
    tmp.mkdir()

    let pub_method = doc.DocMethod(
      doc.MethodFunction,
      "size",
      "box",
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      None,
      false,
      None,
      None)
    let priv_method = doc.DocMethod(
      doc.MethodFunction,
      "_internal",
      "ref",
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      None,
      false,
      None,
      None)

    let entity = doc.DocEntity(
      doc.EntityClass,
      "Foo",
      "mypkg-Foo",
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val Array[doc.DocType] end,
      None,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocField] end,
      recover val Array[doc.DocMethod] end,
      recover val [as doc.DocMethod: pub_method] end,
      recover val Array[doc.DocMethod] end,
      recover val [as doc.DocMethod: priv_method] end,
      None)

    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, false)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("* Foo.size:"),
      "public method missing from method index")
    h.assert_false(
      content.contains("* Foo._internal:"),
      "private method should not be in method index")

    _InfoTestHelper.cleanup(tmp)

class \nodoc\ _TestInfoBackendPrivateMethodIndexIncluded is UnitTest
  """Verify private methods included in Method Index when requested."""
  fun name(): String => "InfoBackend/private-method-index-included"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = FilePath(auth,
      _InfoTestHelper.tmp_dir(h,"priv-meth-incl"))
    tmp.mkdir()

    let pub_method = doc.DocMethod(
      doc.MethodFunction,
      "size",
      "box",
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      None,
      false,
      None,
      None)
    let priv_method = doc.DocMethod(
      doc.MethodFunction,
      "_internal",
      "ref",
      recover val Array[doc.DocTypeParam] end,
      recover val Array[doc.DocParam] end,
      None,
      false,
      None,
      None)

    let entity = doc.DocEntity(
      doc.EntityClass,
      "Foo",
      "mypkg-Foo",
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val Array[doc.DocType] end,
      None,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocField] end,
      recover val Array[doc.DocMethod] end,
      recover val [as doc.DocMethod: pub_method] end,
      recover val Array[doc.DocMethod] end,
      recover val [as doc.DocMethod: priv_method] end,
      None)

    let program = _InfoTestHelper.program("mypkg",
      recover val [as doc.DocEntity: entity] end)

    doc.InfoBackend.generate(program, tmp, true)?

    let content = _InfoTestHelper.read_file(tmp,"mypkg.info")?

    h.assert_true(
      content.contains("* Foo.size:"),
      "public method missing from method index")
    h.assert_true(
      content.contains("* Foo._internal:"),
      "private method should be in method index with include_private=true")

    _InfoTestHelper.cleanup(tmp)

primitive \nodoc\ _InfoTestHelper
  fun tmp_dir(h: TestHelper, suffix: String): String =>
    let home =
      try _get_env_var(h.env.vars, "HOME")? else "/tmp" end
    home + "/tmp/pony-doc-test-" + suffix

  fun _get_env_var(
    vars: (Array[String val] val | None),
    key: String)
    : String ?
  =>
    match vars
    | let env_vars: Array[String val] val =>
      let prefix: String val = key + "="
      for pair in env_vars.values() do
        if pair.at(prefix) then
          return pair.substring(prefix.size().isize())
        end
      end
    end
    error

  fun program(
    pkg_name: String,
    entities: Array[doc.DocEntity] val)
    : doc.DocProgram
  =>
    let mod = doc.DocModule(
      pkg_name,
      None,
      entities,
      recover val Array[doc.DocEntity] end,
      recover val Array[doc.DocSourceFile] end)
    doc.DocProgram(
      pkg_name,
      recover val [as doc.DocModule: mod] end)

  fun entity(
    name': String,
    tqfn': String,
    kind': doc.EntityKind,
    methods: Array[doc.DocMethod] val =
      recover val Array[doc.DocMethod] end)
    : doc.DocEntity
  =>
    doc.DocEntity(
      kind',
      name',
      tqfn',
      recover val Array[doc.DocTypeParam] end,
      None,
      recover val Array[doc.DocType] end,
      None,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocField] end,
      recover val Array[doc.DocMethod] end,
      methods,
      recover val Array[doc.DocMethod] end,
      recover val Array[doc.DocMethod] end,
      None)

  fun read_file(dir: FilePath, file_name: String): String ? =>
    let path = dir.join(file_name)?
    match OpenFile(path)
    | let file: File =>
      let content = file.read_string(file.size())
      file.dispose()
      content
    else
      error
    end

  fun cleanup(dir: FilePath) =>
    dir.remove()
