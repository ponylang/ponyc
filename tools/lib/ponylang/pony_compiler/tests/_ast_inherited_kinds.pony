use "files"
use "pony_test"

use "../pony_compiler"

primitive \nodoc\ _InheritedKindsFixture
  """
  Compile the inherited_kinds fixture to the expr pass: trait T's method mk
  holds an object literal providing trait W, and class C inherits mk; trait G
  has a guarded method g, which class D inherits.
  """
  fun apply(h: TestHelper): (SourceFile | None) =>
    let source_dir = Path.join(Path.dir(__loc.file()), "inherited_kinds")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return None
      end
    match \exhaustive\ Compiler.compile(
      FilePath(FileAuth(h.env.root), source_dir),
      [pony_path] where limit = PassExpr)
    | let p: Program =>
      try
        return (p.module() as Module).file() as SourceFile
      end
      h.fail("no source file")
    | let e: Array[Error] val =>
      h.fail("compilation failed")
    end
    None

class \nodoc\ _MatchCounter is ASTVisitor
  var count: USize = 0

  fun ref visit(ast: AST box): VisitResult =>
    if ast.id() == TokenIds.tk_match() then
      count = count + 1
    end
    Continue

class \nodoc\ iso _ASTVisitSkipsInheritedCode is UnitTest
  """
  The fixture has two matches: one written in T's literal and one in G's
  guarded g. Visiting the file meets each once: the first in the class T's
  check made from the literal, not again in C's copy of mk or the class that
  copy made; the second in G's own g, not again in D's copy.
  """
  fun name(): String => "ast/visit-skips-inherited-code"

  fun apply(h: TestHelper) =>
    match _InheritedKindsFixture(h)
    | let file: SourceFile =>
      let counter: _MatchCounter ref = _MatchCounter
      file.ast.visit(counter)
      h.assert_eq[USize](2, counter.count)
    end

class \nodoc\ iso _ASTInheritedKinds is UnitTest
  """
  Which nodes are inherited code: the class C's copy of mk made from T's
  literal, the methods of the literal classes holding W's checked w, and C's
  copy of mk and D's copies of g. The class T's check made from its literal,
  and T's and G's own methods, are not.
  """
  fun name(): String => "ast/inherited-kinds"

  fun apply(h: TestHelper) =>
    match _InheritedKindsFixture(h)
    | let file: SourceFile =>
      var trait_classes: USize = 0
      var copy_classes: USize = 0
      var trait_views: USize = 0
      var copies = Array[String]

      for entity in file.ast.children() do
        let entity_name = _name(entity, 0)
        let trait_class = entity.has_annotation("ponyint_anon_trait")
        if trait_class then
          trait_classes = trait_classes + 1
          h.assert_false(entity.has_annotation("ponyint_anon_copy"))
          h.assert_false(entity.is_inherited_code())
        end
        if entity.has_annotation("ponyint_anon_copy") then
          copy_classes = copy_classes + 1
          h.assert_true(entity.is_inherited_code())
        end

        try
          for member in entity(4)?.children() do
            let methods =
              if member.id() == TokenIds.tk_methodgroup() then
                member.children()
              else
                [member].values()
              end

            for m in methods do
              if m.has_annotation("ponyint_traitview") then
                trait_views = trait_views + 1
                h.assert_eq[String]("w", _name(m, 1))
                h.assert_true(m.is_inherited_code())
              elseif m.is_inherited_code() and (not trait_class) and
                (not entity.has_annotation("ponyint_anon_copy"))
              then
                copies.push(entity_name + "." + _name(m, 1))
              end
            end
          end
        end
      end

      h.assert_eq[USize](1, trait_classes)
      h.assert_eq[USize](1, copy_classes)
      h.assert_eq[USize](2, trait_views)
      h.assert_array_eq[String](["C.mk"; "D.g"; "D.g"], copies)
    end

  fun _name(node: AST box, index: USize): String =>
    try
      (node(index)?.token_value() as String)
    else
      ""
    end
