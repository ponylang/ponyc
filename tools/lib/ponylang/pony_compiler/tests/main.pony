use "collections"
use "files"
use "itertools"
use "pony_test"

use "../pony_compiler"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  fun tag tests(test: PonyTest) =>
    test(_CompileSimple)
    test(_CompileRepeatedly)
    test(_CompileErrorTest)
    test(_PositionIndexFind)
    test(_DefinitionTest)
    test(_EqHashTest)
    test(_ParallelCompilerTest)

class \nodoc\ iso _CompileSimple is UnitTest
  fun name(): String => "compile/simple"

  fun apply(h: TestHelper) =>
    let source_dir =
      Path.join(Path.dir(__loc.file()), "simple")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match \exhaustive\ Compiler.compile(
      FilePath(FileAuth(h.env.root), source_dir),
      [pony_path] where limit = PassFinaliser)
    | let p: Program =>
      try
        let pkg = p.package() as Package
        h.assert_eq[String](source_dir, pkg.path)
      else
        h.fail("No package, huh?")
      end
    | let e: Array[Error] val =>
      h.fail(
        "Compiling the simple example failed with: " +
        try
          e(0)?.msg
        else
          "0 errors"
        end)
    end

class \nodoc\ iso _CompileRepeatedly is UnitTest
  fun name(): String => "compile/repeatedly"

  fun apply(h: TestHelper) =>
    let source_dir =
      Path.join(Path.dir(__loc.file()), "simple")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    var rounds: USize = 3
    while rounds > 0 do
      match \exhaustive\ Compiler.compile(
        FilePath(FileAuth(h.env.root), source_dir),
        [pony_path] where limit = PassFinaliser)
      | let p: Program =>
        try
          let pkg = p.package() as Package
          h.assert_eq[String](source_dir, pkg.path)
        else
          h.fail("No package, huh?")
        end
      | let e: Array[Error] val =>
        h.fail(
          "Compiling the simple example failed with: " +
          try
            e(0)?.msg
          else
            "0 errors"
          end)
      end
      rounds = rounds - 1
    end

class \nodoc\ iso _PositionIndexFind is UnitTest
  let expected: Array[(USize, USize, TokenId)] val =
    [
      (1, 1, TokenIds.tk_use()) // use
      (1, 5, TokenIds.tk_string()) // use url
      (1, 17, TokenIds.tk_string()) // end of use url
      (4, 2, TokenIds.tk_actor()) // actor keyword
      (4, 7, TokenIds.tk_id()) // actor name
      (5, 4, TokenIds.tk_fvar()) // var field
      (5, 7, TokenIds.tk_fvar()) // field name
      (7, 3, TokenIds.tk_new()) // constructor begin
      (7, 7, TokenIds.tk_new()) // constructor name
      (7, 16, TokenIds.tk_param()) // param name
      (7, 20, TokenIds.tk_nominal()) // param type
      (8, 6, TokenIds.tk_fvarref()) // field ref
      (10, 11, TokenIds.tk_typeref()) // String type ref
      (10, 21, TokenIds.tk_newref()) // String.create
      (10, 23, TokenIds.tk_int()) // int arg
      (10, 24, TokenIds.tk_int()) // int arg
      (10, 28, TokenIds.tk_funchain()) // .>
      (10, 31, TokenIds.tk_funchain()) // append
      (10, 38, TokenIds.tk_string()) // "snot"
      (12, 7, TokenIds.tk_paramref()) // env
      (12, 8, TokenIds.tk_fletref()) // env.out dot
      (12, 9, TokenIds.tk_fletref()) // env.out
      (12, 12, TokenIds.tk_beref()) // .print dot
      (12, 13, TokenIds.tk_beref()) // print
      (12, 18, TokenIds.tk_call()) // (
      (12, 19, TokenIds.tk_fvarref()) // name field
      (14, 3, TokenIds.tk_trait()) // trait
      (14, 9, TokenIds.tk_id()) // trait name
      (15, 5, TokenIds.tk_fun()) // fun keyword
      (15, 7, TokenIds.tk_fun()) // fun name
      (15, 14, TokenIds.tk_nominal()) // return type
      (17, 3, TokenIds.tk_fun()) // fun keyword
      (17, 7, TokenIds.tk_fun()) // fun name
      (17, 16, TokenIds.tk_nominal()) // return type
      (17, 24, TokenIds.tk_funref()) // ref to bla
      (17, 27, TokenIds.tk_call()) // call args start
      (17, 30, TokenIds.tk_call()) // desugared ==
      (17, 33, TokenIds.tk_newref()) // constructor ref
      (17, 35, TokenIds.tk_call()) // constructor call
      (17, 36, TokenIds.tk_int()) // integer argument
      (19, 1, TokenIds.tk_class()) // class keyword
      (19, 7, TokenIds.tk_id()) // class name
      (19, 14, TokenIds.tk_nominal()) // provided type
      (20, 3, TokenIds.tk_string()) // docstring line 1
      (21, 1, TokenIds.tk_string()) // docstring 2 begin
      (21, 11, TokenIds.tk_string()) // docstring 2 end
      (22, 5, TokenIds.tk_string()) // triple-quote end
      (24, 3, TokenIds.tk_fvar()) // var keyword
      (24, 7, TokenIds.tk_fvar()) // var name
      (24, 18, TokenIds.tk_nominal()) // Array
      (24, 24, TokenIds.tk_nominal()) // String type arg
      (24, 32, TokenIds.tk_nominal()) // the iso part
      (24, 36, TokenIds.tk_uniontype()) // union pipe
      (24, 38, TokenIds.tk_nominal()) // None init
      (25, 3, TokenIds.tk_embed()) // embed keyword
      (25, 9, TokenIds.tk_embed()) // embed name
      (25, 12, TokenIds.tk_nominal()) // String type
      // String type ref in constructor call init
      (25, 21, TokenIds.tk_typeref())
      // constructor ref, the dot
      (25, 27, TokenIds.tk_newref())
      (25, 28, TokenIds.tk_newref()) // constructor ref
      (25, 34, TokenIds.tk_call()) // constructor call
      (25, 35, TokenIds.tk_int()) // binary int arg
      (25, 38, TokenIds.tk_int()) // binary int end
      (26, 7, TokenIds.tk_flet()) // immutable field
      (28, 20, TokenIds.tk_newref()) // F32 ctor ref
      (28, 23, TokenIds.tk_call()) // constructor call
      (28, 24, TokenIds.tk_float()) // float start
      (28, 32, TokenIds.tk_float()) // float end
      (28, 35, TokenIds.tk_funref()) // .u8() dot
      // -1 transformed to 1.neg()
      (30, 50, TokenIds.tk_call())
      // syntax rewrite removes this node
      // (30, 19, TokenIds.tk_object())
      (32, 15, TokenIds.tk_flet())
      (33, 15, TokenIds.tk_fun())
      (34, 11, TokenIds.tk_match())
      (34, 21, TokenIds.tk_funref())
      (35, 11, TokenIds.tk_match_capture())
      (35, 15, TokenIds.tk_id())
      (35, 18, TokenIds.tk_nominal())
      (35, 29, TokenIds.tk_call()) // call to s.gt(0)
      (35, 36, TokenIds.tk_letref())
      // (36, 30, TokenIds.tk_chain())
    ]

  fun name(): String => "position-index/find"

  fun apply(h: TestHelper) =>
    let source_dir =
      Path.join(Path.dir(__loc.file()), "constructs")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match \exhaustive\ Compiler.compile(
      FilePath(FileAuth(h.env.root), source_dir),
      [pony_path] where limit = PassFinaliser)
    | let p: Program =>
      try
        let pkg = p.package() as Package
        h.assert_eq[String](source_dir, pkg.path)
        let main_pony_path =
          Path.join(source_dir, "main.pony")
        try
          let module =
            pkg.find_module(main_pony_path) as Module
          let index = module.create_position_index()
          index.debug(h.env.out)

          for (line, column, expected_token_id) in
            expected.values()
          do
            match \exhaustive\
              index.find_node_at(line, column)
            | let ast: AST box =>
              h.assert_eq[String val](
                TokenIds.string(expected_token_id),
                TokenIds.string(ast.id()),
                "Found wrong node at "
                  + line.string() + ":"
                  + column.string() + ": "
                  + ast.debug())
            | None =>
              h.fail(
                "No AST node found at "
                  + line.string() + ":"
                  + column.string())
              return
            end

          end
        else
          h.fail(
            "No module with file " + main_pony_path)
          return
        end
      else
        h.fail("No package, huh?")
        return
      end
    | let e: Array[Error] val =>
      h.fail(
        "Compiling the constructs example failed with: " +
        try
          e(0)?.msg + " " + e(0)?.position.string()
        else
          "0 errors"
        end)
    end

class \nodoc\ _DefinitionTest is UnitTest
  let expected:
    Array[(Position, Array[Position] val, TokenId)] val =
    [
      // nominal reference to type in a different file
      (Position(7, 20),
        [Position(4, 1)], TokenIds.tk_class())
      (Position(8, 5),
        [Position(5, 3)], TokenIds.tk_fvar())
      // constructor ref in a different file
      (Position(10, 16),
        [Position(54, 3)], TokenIds.tk_new())
      // function ref in a different file
      (Position(10, 30),
        [Position(926, 3)], TokenIds.tk_fun())
      (Position(12, 6),
        [Position(7, 14)], TokenIds.tk_param())
      // field ref in a different file
      (Position(12, 10),
        [Position(19, 3)], TokenIds.tk_flet())
      // behavior ref in a different file
      (Position(12, 15),
        [Position(20, 3)], TokenIds.tk_be())
      // function on a union type receiver
      (Position(71, 35),
        [Position(53, 3); Position(59, 3)],
        TokenIds.tk_fun())
      // function on an isect type receiver
      (Position(72, 34),
        [Position(45, 3); Position(48, 3)],
        TokenIds.tk_fun())
      // tuple element ref resolves to whole tuple def
      (Position(70, 32),
        [Position(63, 3)], TokenIds.tk_flet())
      // type param ref
      (Position(79, 10),
        [Position(78, 24)], TokenIds.tk_typeparam())
      // type ref within a newberef
      (Position(82, 9),
        [Position(78, 24)], TokenIds.tk_typeparam())
      // package ref
      (Position(87, 5),
        [Position(1, 1)], TokenIds.tk_module())
      // constructor ref; TypeName desugars to .create()
      (Position(97, 9),
        [Position(91, 1)], TokenIds.tk_new())
      // method ref of the own type
      (Position(98, 7),
        [Position(103, 3)], TokenIds.tk_fun())
      // this reference
      (Position(100, 8),
        [Position(94, 1)], TokenIds.tk_class())
      // method referred to via this.method_name
      (Position(100, 12),
        [Position(106, 3)], TokenIds.tk_fun())
    ]

  fun name(): String => "definition/test"

  fun apply(h: TestHelper) =>
    let source_dir =
      Path.join(Path.dir(__loc.file()), "constructs")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match \exhaustive\ Compiler.compile(
      FilePath(FileAuth(h.env.root), source_dir),
      [pony_path] where limit = PassFinaliser)
    | let p: Program =>
      try
        let pkg = p.package() as Package
        h.assert_eq[String](source_dir, pkg.path)
        let main_pony_path =
          Path.join(source_dir, "main.pony")
        try
          let module =
            pkg.find_module(main_pony_path) as Module
          let index = module.create_position_index()
          index.debug(h.env.out)

          for (ref_pos, expected_def_positions,
            expected_def_token_id) in expected.values()
          do
            match \exhaustive\ index.find_node_at(
              ref_pos.line(), ref_pos.column())
            | let ast: AST box =>
              let definitions: Array[AST] =
                ast.definitions()
              if definitions.size() == 0 then
                h.fail(
                  "No definition for node: "
                    + ast.debug())
                return
              end
              h.assert_eq[USize](
                definitions.size(),
                expected_def_positions.size(),
                "Expected to find "
                  + expected_def_positions
                    .size().string()
                  + " definitions, found: "
                  + definitions.size().string())
              let iter =
                Iter[Position](
                  expected_def_positions.values())
                .zip[AST](definitions.values())
              for (expected_def_pos, definition) in
                iter
              do
                h.assert_eq[Position](
                  expected_def_pos,
                  definition.position(),
                  "Definition at wrong position "
                    + definition.debug())
                h.assert_eq[String val](
                  TokenIds.string(
                    expected_def_token_id),
                  TokenIds.string(definition.id()),
                  "Found wrong node " + ast.debug())
              end
            | None =>
              h.fail(
                "No AST node found at "
                  + ref_pos.string())
              return
            end

          end
        else
          h.fail(
            "No module with file " + main_pony_path)
          return
        end
      else
        h.fail("No package, huh?")
        return
      end
    | let e: Array[Error] val =>
      h.fail(
        "Compiling the constructs example failed with: " +
        try
          e(0)?.msg + " " + e(0)?.position.string()
        else
          "0 errors"
        end)
    end

class \nodoc\ _EqHashTest is UnitTest
  fun name(): String => "eq/hash/test"

  fun apply(h: TestHelper) =>
    let source_dir =
      Path.join(Path.dir(__loc.file()), "constructs")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match \exhaustive\ Compiler.compile(
      FilePath(FileAuth(h.env.root), source_dir),
      [pony_path] where limit = PassFinaliser)
    | let p: Program =>
      for package in p.packages() do
        for module in package.modules() do
          let module_ast: AST box = module.ast
          module_ast.visit(
            object is ASTVisitor
              var _last: (AST box | None) = None

              fun ref visit(
                ast: AST box): VisitResult
              =>
                match _last
                | let last: AST box =>
                  h.assert_ne[AST box](last, ast)
                  h.assert_ne[USize](
                    last.hash(), ast.hash())
                else
                  _last = ast
                end
                Continue
            end
          )
        end
      end
      try
        let pkg = p.package() as Package
        let main_pony_path =
          Path.join(source_dir, "main.pony")
        let main_module =
          pkg.find_module(main_pony_path) as Module
        let index =
          main_module.create_position_index()
        let env_ref =
          index.find_node_at(12, 5) as AST box
        let env =
          index.find_node_at(7, 14) as AST box
        let env_def = env_ref.definitions()(0)?
        h.assert_eq[AST box](
          env, env_def, "SAME nodes do not eq")
        try
          let main_ast =
            index.find_node_at(4, 7) as AST box
          let found_main =
            main_module.ast.find_in_scope("Main")
              as AST box
          h.assert_eq[AST box](
            main_ast,
            found_main,
            "SAME MAIN nodes do not eq")
        end
      else
        h.fail("Couldnt get env node")
      end
    | let e: Array[Error] val =>
      h.fail(
        "Compiling the constructs example failed with: " +
        try
          e(0)?.msg + " " + e(0)?.position.string()
        else
          "0 errors"
        end)
    end

type ExpectedMessage is String

class \nodoc\ _ExpectedCompileErrors
  let folder: String
  let expected_errors:
    Array[(String, Position, ExpectedMessage)]

  new create(
    folder': String,
    expected_errors':
      Array[(String, Position, ExpectedMessage)])
  =>
    folder = folder'
    expected_errors = expected_errors'

class \nodoc\ _CompileErrorTest is UnitTest
  let expected_errors: Array[_ExpectedCompileErrors] =
    [
      _ExpectedCompileErrors(
        "compile_errors_01",
        [
          ("main.pony", Position(5, 25),
            "can't find definition of 'Badger'")
        ])
      _ExpectedCompileErrors(
        "compile_errors_02",
        [
          ("main.pony", Position(5, 17),
            "this parameter must be sendable"
              + " (iso, val or tag)")
        ])
      _ExpectedCompileErrors(
        "compile_errors_03",
        [
          ("main.pony", Position(3, 9),
            "can't find declaration of 'frobnicate'")
          ("main.pony", Position(6, 5),
            "can't find declaration of 'foo'")
          ("main.pony", Position(6, 9),
            "left side must be something"
              + " that can be assigned to")
        ])
      _ExpectedCompileErrors(
        "compile_errors_04",
        [
          ("main.pony", Position(1, 5),
            "Literal doesn't terminate")
        ])
    ]

  fun name(): String => "compile/errors"

  fun apply(h: TestHelper) ? =>
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end

    for expected_errs in
      this.expected_errors.values()
    do
      let source_dir =
        Path.join(
          Path.dir(__loc.file()),
          expected_errs.folder)

      match \exhaustive\ Compiler.compile(
        FilePath(FileAuth(h.env.root), source_dir),
        [pony_path] where limit = PassFinaliser)
      | let p: Program =>
        h.fail(
          "Program successfully compiled,"
            + " although we expected it to fail")
      | let e: Array[Error] val =>
        h.assert_eq[USize](
          e.size(),
          expected_errs.expected_errors.size(),
          "Expected "
            + expected_errs
              .expected_errors.size().string()
            + " Errors, got "
            + e.size().string())
        for i in Range(0, e.size()) do
          let err = e(i)?
          (let expected_file,
            let expected_position,
            let expected_message) =
            expected_errs.expected_errors(i)?
          h.assert_true(
            err.file isnt None,
            "Error has no file")
          h.assert_eq[String](
            Path.base(err.file as String),
            expected_file,
            "Error file "
              + (err.file as String)
              + " does not match expected file "
              + expected_file)
          h.assert_eq[Position](
            err.position,
            expected_position,
            "Error position "
              + err.position.string()
              + " does not match expected "
              + "position "
              + expected_position.string())
          h.assert_eq[String](
            err.msg,
            expected_message,
            "Error message \""
              + err.msg
              + "\" doesn't match expected "
              + "message \""
              + expected_message + "\"")
        end
      end
    end

actor \nodoc\ ParallelCompiler
  new create(
    token: String,
    source_dir: FilePath,
    h: TestHelper)
  =>
    compile(token, source_dir, h)

  be compile(
    token: String,
    source_dir: FilePath,
    h: TestHelper)
  =>
    h.log("Starting Compiler " + token)
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match \exhaustive\ Compiler.compile(
      source_dir,
      [pony_path]
      where limit = PassFinaliser,
        verbosity = VerbosityInfo)
    | let p: Program =>
      h.log("Compiler " + token + " done.")
      for pkg in p.packages() do
        for mod in pkg.modules() do
          try
            let mod_last =
              (mod.ast.last_child() as AST)
            h.log(
              mod.file + ": " + mod_last.debug())
          end
        end
      end
      h.complete_action(token)
    | let e: Array[Error] val =>
      h.fail(
        "Compiling the constructs example failed with: " +
        try
          e(0)?.msg + " " + e(0)?.position.string()
        else
          "0 errors"
        end)
    end

class \nodoc\ _ParallelCompilerTest is UnitTest
  fun name(): String => "parallel/compilation"

  fun apply(h: TestHelper) =>
    h.long_test(60_000_000_000)
    let source_dir =
      FilePath(
        FileAuth(h.env.root),
        Path.join(
          Path.dir(__loc.file()), "constructs"))
    for i in Range(0, 10) do
      let token: String val = i.string()
      h.expect_action(token)
      ParallelCompiler.create(
        token, source_dir, h)
    end
