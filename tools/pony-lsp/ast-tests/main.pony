use "debug"
use "files"
use "pony_test"
use "../ast"
use "itertools"

actor \nodoc\ Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  fun tag tests(test: PonyTest) =>
    test(_CompileSimple)
    test(_CompileRepeatedly)
    test(_PositionIndexFind)
    test(_DefinitionTest)
    // test(_FindTypes)

class \nodoc\ iso _CompileSimple is UnitTest
  fun name(): String => "compile/simple"

  fun apply(h: TestHelper) =>
    let source_dir = Path.join(Path.dir(__loc.file()), "simple")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match Compiler.compile(FilePath(FileAuth(h.env.root), source_dir), [pony_path])
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
    let source_dir = Path.join(Path.dir(__loc.file()), "simple")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    var rounds: USize = 3
    while rounds > 0 do
      match Compiler.compile(FilePath(FileAuth(h.env.root), source_dir), [pony_path])
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

// class \nodoc\ iso _FindTypes is UnitTest
//   let expected: Array[(USize, USize, String val)] val = [
//     (4, 7, "String val") // field name
//     (6, 16, "Env val")   // param name
//     (6, 20, "Env val")   // param type
//     (7, 6, "String val") // field ref
//     (9, 11, "String val")  // String type ref
//     (9, 21, "String ref^") // String.create
//     (9, 23, "USize val")   // literal int argument to String.create
//     (9, 24, "USize val")   // literal int argument to String.create
//   ]

//   fun name(): String => "find-types"

//   fun apply(h: TestHelper) =>
//     let source_dir = Path.join(Path.dir(__loc.file()), "constructs")
//     let pony_path =
//       try
//         PonyPath(h.env) as String
//       else
//         h.fail("PONYPATH not set")
//         return
//       end
//     match Compiler.compile(FilePath(FileAuth(h.env.root), source_dir), [pony_path])
//     | let p: Program =>
//       try
//         let pkg = p.package() as Package
//         h.assert_eq[String](source_dir, pkg.path)
//         let main_pony_path = Path.join(source_dir, "main.pony")
//         try
//           let module = pkg.find_module(main_pony_path) as Module

//           for (line, column, expected_type) in expected.values() do
//             match module.find_node_at(line, column)
//             | let ast: AST box =>
//               Debug("FOUND " + TokenIds.string(ast.id()))
//               match Types.get_ast_type(ast)
//               | let found_type: String =>
//                 h.assert_eq[String val](expected_type, found_type, "Error at " + line.string() + ":" + column.string())
//               | None =>
//                 h.fail("No Type found for node " + TokenIds.string(ast.id()) + " at " + line.string() + ":" + column.string())
//                 return
//               end
//             | None =>
//               h.fail("No AST node found at " + line.string() + ":" + column.string())
//               return
//             end

//           end
//         else
//           h.fail("No module with file " + main_pony_path)
//           return
//         end
//       else
//         h.fail("No package, huh?")
//         return
//       end
//     | let e: Array[Error] =>
//       h.fail(
//         "Compiling the constructs example failed with: " +
//         try
//           e(0)?.msg + " " + e(0)?.line.string() + ":" + e(0)?.pos.string()
//         else
//           "0 errors"
//         end)
//     end

class iso _PositionIndexFind is UnitTest
  let expected: Array[(USize, USize, TokenId)] val = [
    (1, 1, TokenIds.tk_use()) // use
    (1, 5, TokenIds.tk_string()) // use url
    (1, 17, TokenIds.tk_string()) // end of use url
    (4, 2, TokenIds.tk_actor()) // actor keyword
    (4, 7, TokenIds.tk_id()) // actor name
    (5, 4, TokenIds.tk_fvar()) // var field
    (5, 7, TokenIds.tk_fvar()) // field name
    (7, 3, TokenIds.tk_new()) // constructor begin
    (7, 7, TokenIds.tk_new())  // constructor name
    (7, 16, TokenIds.tk_param())   // param name
    (7, 20, TokenIds.tk_nominal())   // param type
    (8, 6, TokenIds.tk_fvarref()) // field ref
    (10, 11, TokenIds.tk_typeref())  // String type ref
    (10, 21, TokenIds.tk_newref()) // String.create
    (10, 23, TokenIds.tk_int())   // literal int argument to String.create
    (10, 24, TokenIds.tk_int())   // literal int argument to String.create
    (10, 28, TokenIds.tk_funchain()) // .>
    (10, 31, TokenIds.tk_funchain()) // append
    (10, 38, TokenIds.tk_string()) // "snot"
    (12, 7, TokenIds.tk_paramref()) // env
    (12, 8, TokenIds.tk_fletref()) // env.out <- the dot
    (12, 9, TokenIds.tk_fletref()) // env.out <- out
    (12, 12, TokenIds.tk_beref()) // env.out.print <- the second dot
    (12, 13, TokenIds.tk_beref()) // print
    (12, 18, TokenIds.tk_call()) // (
    (12, 19, TokenIds.tk_fvarref()) // name <- field reference as param
    (14, 3, TokenIds.tk_trait()) // trait
    (14, 9, TokenIds.tk_id()) // trait name
    (15, 5, TokenIds.tk_fun()) // fun keyword
    (15, 7, TokenIds.tk_fun()) // fun name
    (15, 14, TokenIds.tk_nominal()) // return type
    (17, 3, TokenIds.tk_fun()) // fun keyword
    (17, 7, TokenIds.tk_fun()) // fun name
    (17, 16, TokenIds.tk_nominal()) // return type
    (17, 24, TokenIds.tk_funref()) // reference to bla
    (17, 27, TokenIds.tk_call()) // start of call arguments
    (17, 30, TokenIds.tk_call()) // desugared `==` operator to call to `eq`
    (17, 33, TokenIds.tk_newref()) // reference to the constructor
    (17, 35, TokenIds.tk_call()) // call of the constructor
    (17, 36, TokenIds.tk_int()) // integer argument
    (19, 1, TokenIds.tk_class()) // class keyword
    (19, 7, TokenIds.tk_id()) // class name
    (19, 14, TokenIds.tk_nominal()) // provided type `Snot`
    (20, 3, TokenIds.tk_string()) // docstring first line
    (21, 1, TokenIds.tk_string()) // docstring second line beginning
    (21, 11, TokenIds.tk_string()) // docstring second line end
    (22, 5, TokenIds.tk_string()) // last ending quote of triple-quote
    (24, 3, TokenIds.tk_fvar()) // var keyword
    (24, 7, TokenIds.tk_fvar()) // var name
    (24, 18, TokenIds.tk_nominal()) // Array
    (24, 24, TokenIds.tk_nominal()) // String type argument
    (24, 32, TokenIds.tk_nominal()) // the iso part
    (24, 36, TokenIds.tk_uniontype()) // the union pipe
    (24, 38, TokenIds.tk_nominal()) // None initializer
    (25, 3, TokenIds.tk_embed()) // embed keyword
    (25, 9, TokenIds.tk_embed()) // embed name
    (25, 12, TokenIds.tk_nominal()) // String type
    (25, 21, TokenIds.tk_typeref()) // String type ref in initializer as part of constructor call
    (25, 27, TokenIds.tk_newref()) // reference to the constructor, the dot
    (25, 28, TokenIds.tk_newref()) // reference to the constructor
    (25, 34, TokenIds.tk_call()) // constructor call
    (25, 35, TokenIds.tk_int()) // binary int argument
    (25, 38, TokenIds.tk_int()) // binary int argument - end
    (26, 7, TokenIds.tk_flet()) // immutable field name
    (28, 20, TokenIds.tk_newref()) // reference to F32 constructor
    (28, 23, TokenIds.tk_call()) // constructor call
    (28, 24, TokenIds.tk_float()) // float literal start
    (28, 32, TokenIds.tk_float()) // float literal end
    (28, 35, TokenIds.tk_funref()) // .u8() - the dot
    (30, 50, TokenIds.tk_call()) // the -1, which was transformed to 1.neg()
    //(30, 19, TokenIds.tk_object()) // the syntax rewrite of objects and lambdas removes this node
    (32, 15, TokenIds.tk_flet())
    (33, 15, TokenIds.tk_fun())
    (34, 11, TokenIds.tk_match())
    (34, 21, TokenIds.tk_funref())
    (35, 11, TokenIds.tk_match_capture())
    (35, 15, TokenIds.tk_id())
    (35, 18, TokenIds.tk_nominal())
    (35, 29, TokenIds.tk_call()) // call to s.gt(0)
    (35, 36, TokenIds.tk_id())
    //(36, 30, TokenIds.tk_chain())
  ]


  fun name(): String => "position-index/find"

  fun apply(h: TestHelper) =>
    let source_dir = Path.join(Path.dir(__loc.file()), "constructs")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match Compiler.compile(FilePath(FileAuth(h.env.root), source_dir), [pony_path])
    | let p: Program =>
      try
        let pkg = p.package() as Package
        h.assert_eq[String](source_dir, pkg.path)
        let main_pony_path = Path.join(source_dir, "main.pony")
        try
          let module = pkg.find_module(main_pony_path) as Module
          let index = module.create_position_index()
          index.debug(h.env.out)

          for (line, column, expected_token_id) in expected.values() do
            match index.find_node_at(line, column)
            | let ast: AST box =>
              h.assert_eq[String val](
                TokenIds.string(expected_token_id),
                TokenIds.string(ast.id()),
                "Found wrong node at " + line.string() + ":" + column.string() + ": " + ast.debug())
            | None =>
              h.fail("No AST node found at " + line.string() + ":" + column.string())
              return
            end

          end
        else
          h.fail("No module with file " + main_pony_path)
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
  let expected: Array[(Position, Array[Position] val, TokenId)] val = [
    (Position.create(7, 20), [Position.create(4, 1)], TokenIds.tk_class()) // nominal reference to type in a different file
    (Position.create(8, 5), [Position.create(5, 3)], TokenIds.tk_fvar())
    (Position.create(10, 16), [Position.create(54, 3)], TokenIds.tk_new()) // reference to constructor in a different file
    (Position.create(10, 30), [Position.create(921, 3)], TokenIds.tk_fun()) // reference to function in a different file
    (Position.create(12, 6), [Position.create(7, 14)], TokenIds.tk_param())
    (Position.create(12, 10), [Position.create(19, 3)], TokenIds.tk_flet()) // reference to behavior in a different file
    (Position.create(12, 15), [Position.create(20, 3)], TokenIds.tk_be()) // reference to behavior in a different file
    (Position.create(71, 35), [Position.create(53, 3); Position.create(59, 3)], TokenIds.tk_fun()) // function on a union type receiver
    (Position.create(72, 34), [Position.create(45, 3); Position.create(48, 3)], TokenIds.tk_fun()) // function on an isect type receiver
    (Position.create(70, 32), [Position.create(63, 3)], TokenIds.tk_flet()) // tuple element reference - resolves to definition of the whole tuple
    (Position.create(79, 10), [Position.create(78, 24)], TokenIds.tk_typeparam()) // type param ref
    (Position.create(82, 9), [Position.create(78, 24)], TokenIds.tk_typeparam()) // type ref within a newberef
    (Position.create(87, 5), [Position.create(1, 1)], TokenIds.tk_module()) // package ref
    (Position.create(97, 9), [Position.create(91, 1)], TokenIds.tk_new()) // reference to constructor for `TypeName` which desugars to `TypeName.create()`
  ]
  fun name(): String => "definition/test"
  fun apply(h: TestHelper) =>
    let source_dir = Path.join(Path.dir(__loc.file()), "constructs")
    let pony_path =
      try
        PonyPath(h.env) as String
      else
        h.fail("PONYPATH not set")
        return
      end
    match Compiler.compile(FilePath(FileAuth(h.env.root), source_dir), [pony_path])
    | let p: Program =>
      try
        let pkg = p.package() as Package
        h.assert_eq[String](source_dir, pkg.path)
        let main_pony_path = Path.join(source_dir, "main.pony")
        try
          let module = pkg.find_module(main_pony_path) as Module
          let index = module.create_position_index()
          index.debug(h.env.out)

          for (ref_pos, expected_def_positions, expected_def_token_id) in expected.values() do
            match index.find_node_at(ref_pos.line(), ref_pos.column())
            | let ast: AST box =>
              let definitions: Array[AST] = ast.definitions()
              if definitions.size() == 0 then
                h.fail("No definition for node: " + ast.debug())
                return
              end
              h.assert_eq[USize](definitions.size(), expected_def_positions.size(),
                "Expected to find " + expected_def_positions.size().string() +
                "definitions, found: " + definitions.size().string())
              let iter = Iter[Position](expected_def_positions.values()).zip[AST](definitions.values())
              for (expected_def_pos, definition) in iter do
                h.assert_eq[Position](expected_def_pos, definition.position(),
                "Definition at wrong position " + definition.debug())
                h.assert_eq[String val](
                  TokenIds.string(expected_def_token_id),
                  TokenIds.string(definition.id()),
                  "Found wrong node " + ast.debug()
                )
              end
            | None =>
              h.fail("No AST node found at " + ref_pos.string())
              return
            end

          end
        else
          h.fail("No module with file " + main_pony_path)
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
