use "collections"
use "files"
use "itertools"
use "pony_test"
use "../workspace"

primitive \nodoc\ _WorkspaceScannerTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(WsTest(_WorkspaceWithCorralJsonTest))
    test(WsTest(_WorkspaceWithCorralAndTransitiveDeps))
    test(WsTest(_WorkspaceWithCorralAndCyclicDependencies))
    test(WsTest(_WorkspaceWithEscapingCorralPackagePaths))
    test(WsTest(_WorkspaceWithBrokenCorral))
    test(WsTest(_WorkspaceWithoutCorral))
    test(WsTest(_WorkspaceWithoutCorralDir))

class \nodoc\ iso _WorkspaceWithCorralJsonTest is WsUnitTest
  fun name(): String => "workspace/scanner/corral/single"

  fun setup_workspace(ws: WorkspaceBuilder ref) =>
    ws.file("corral.json", """
      {
        "packages": ["foo"],
        "deps": [
          {
            "locator": "github.com/ponylang/package1.git",
            "version": "0.6.2"
          }
        ],
        "info": {
          "description": "WSTest",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "lsp-test"
        }
      }
    """)
    ws.dir("foo")
      .file("main.pony", PonyMainSrc())
      .dir("test").file("main.pony", PonyMainSrc())
    ws.dir("_corral")
      .dir("github_com_ponylang_package1")
      .dir("package1")
      .file("main.pony", PonyMainSrc())

  fun apply(workspace: FilePath, h: TestHelper)? =>
    let scanner = WorkspaceScanner.create(FakeChannel.with_stdout(h.env.out))
    let packages = scanner.scan(workspace)
    // assert we found foo and dependency package1 (with dependencies)
    let p1_name = "github_com_ponylang_package1"
    let p1_path = workspace
      .join("_corral/github_com_ponylang_package1/package1")?.path
    let p1_locator_path = workspace
      .join("_corral/github_com_ponylang_package1")?
    h.assert_eq[USize](
      3,
      packages.size(),
      "Expected scanner to detect 3 packages, got " + packages.size().string()
    )
    for package in packages.values() do
      h.assert_true(
        ["foo"; "foo/test"; "package1"].contains(
          package.name
          where predicate = {(a, b): Bool => a == b}),
        "unexpected package name " + package.name + ".")
      if package.name == "foo" then
        h.assert_eq[String](
          package.folder.path,
          workspace.join("foo")?.path
        )
        AssertDependenciesEq(h, [p1_locator_path], package.dependencies)
      elseif package.name == "foo/test" then
        h.assert_eq[String](
          package.folder.path,
          workspace.join("foo/test")?.path
        )
        AssertDependenciesEq(h, [p1_locator_path], package.dependencies)
      elseif package.name == "package1" then
        h.assert_eq[String](package.folder.path, p1_path)
        AssertDependenciesEq(h, [], package.dependencies)
      end
    end

primitive \nodoc\ AssertDependenciesEq
  fun apply(h: TestHelper, expected: Array[FilePath] box, actual: Array[FilePath] box, loc: SourceLoc = __loc) =>
    h.assert_eq[USize](
      expected.size(),
      actual.size(),
      "Expected " + expected.size().string() + " elements, got " + actual.size().string(),
      loc
    )
    let expected_strs = Iter[FilePath](expected.values()).map[String]({(fp) => fp.path }).collect(Array[String].create(expected.size()))
    let actual_strs = Iter[FilePath](actual.values()).map[String]({(fp) => fp.path }).collect(Array[String].create(actual.size()))
    h.assert_array_eq_unordered[String](expected_strs, actual_strs where loc = loc)



class \nodoc\ iso _WorkspaceWithCorralAndTransitiveDeps is WsUnitTest
  fun name(): String => "workspace/scanner/corral/transitive"

  fun setup_workspace(ws: WorkspaceBuilder) =>
    ws.file("corral.json", """
      {
        "packages": ["foo"],
        "deps": [
          {
            "locator": "github.com/ponylang/package1.git",
            "version": "0.6.2"
          },
          {
            "locator": "github.com/ponylang/package2.git",
            "version": "1.2.3"
          }
        ],
        "info": {
          "description": "ws-test-transitive",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "ws-test"
        }
      }
    """)
    ws.dir("foo").file("main.pony", PonyMainSrc())
    let corral_dir = ws.dir("_corral")
    corral_dir.dir("github_com_ponylang_package1").dir("p1").file("main.pony", PonyMainSrc())
    corral_dir.dir("github_com_ponylang_package2").file("corral.json", """
    {
      "packages": [],
      "deps": [
        {
          "locator": "gitlab.com/ponyslang/package3.git",
          "version": "4.5.6"
        }
      ],
      "info": {
        "description": "ws-test-transitive package 2",
        "homepage": "",
        "license": "",
        "documentation_url": "",
        "version": "0.2.0",
        "name": "ws-test-package2"
      }
    }
    """).dir("p2").file("main.pony", PonyMainSrc())
    corral_dir.dir("gitlab_com_ponyslang_package3").dir("p3").file("schmain.pony", PonyMainSrc())

  fun apply(workspace: FilePath, h: TestHelper)? =>
    let scanner =
      WorkspaceScanner.create(FakeChannel.with_stdout(h.env.out))
    let modules = recover ref scanner.scan(workspace).clone() end
    Sort[Array[ModuleData val], ModuleData val](modules)

    // utils
    let p1_path = workspace
      .join("_corral/github_com_ponylang_package1")?
    let p2_path = workspace
      .join("_corral/github_com_ponylang_package2")?
    let p3_path = workspace
      .join("_corral/gitlab_com_ponyslang_package3")?

    // assert we found 4 modules
    h.assert_eq[USize](4, modules.size())

    // p1 with no dependencies
    let p1 = modules.shift()?
    h.assert_eq[String]("p1", p1.name)
    h.assert_eq[String](
      p1_path.join("p1")?.path,
      p1.folder.path
    )
    AssertDependenciesEq(h, [], p1.dependencies)

    // p2 with p3 as dependency
    let p2 = modules.shift()?
    h.assert_eq[String]("p2", p2.name)
    h.assert_eq[String](
      p2_path.join("p2")?.path,
      p2.folder.path
    )
    AssertDependenciesEq(h, [p3_path], p2.dependencies)

    // p3 without dependencies
    let p3 = modules.shift()?
    h.assert_eq[String]("p3", p3.name)
    h.assert_eq[String](
      p3_path.join("p3")?.path,
      p3.folder.path
    )
    AssertDependenciesEq(h, [], p3.dependencies)

    // - foo  - with correct dependencies (p1, p2, p3)
    let p0 = modules.shift()?
    h.assert_eq[String]("foo", p0.name)
    h.assert_eq[String](workspace.join("foo")?.path, p0.folder.path)
    AssertDependenciesEq(h, [p1_path; p2_path; p3_path], p0.dependencies)




class \nodoc\ iso _WorkspaceWithCorralAndCyclicDependencies is WsUnitTest
  fun name(): String => "workspace/scanner/corral/cyclic"

  fun setup_workspace(ws: WorkspaceBuilder) =>
    ws.file("corral.json", """
      {
        "packages": ["cyclic"],
        "deps": [
          {
            "locator": "github.com/ponylang/package1.git",
            "version": "0.6.2"
          },
          {
            "locator": "github.com/ponylang/package2.git",
            "version": "1.2.3"
          }
        ],
        "info": {
          "description": "ws-test-cyclic",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "ws-test"
        }
      }
    """)
    ws.dir("cyclic").file("main.pony", PonyMainSrc())
    let corral_dir = ws.dir("_corral")
    corral_dir.dir("github_com_ponylang_package1").file("corral.json", """
      {
        "packages": [],
        "deps": [
          {
            "locator": "github.com/ponylang/package2.git",
            "version": "1.2.3"
          }
        ],
        "info": {
          "description": "ws-test-cyclic-p1",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "ws-test"
        }
      }
    """).dir("p1").file("main.pony", PonyMainSrc())
    corral_dir.dir("github_com_ponylang_package2").file("corral.json", """
      {
        "packages": ["p2"],
        "deps": [
          {
            "locator": "github.com/ponylang/package1.git",
            "version": "0.6.2"
          }
        ],
        "info": {
          "description": "ws-test-cyclic-p2",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "ws-test"
        }
      }
    """).dir("p2").file("main.pony", PonyMainSrc())

  fun apply(workspace: FilePath, h: TestHelper)? =>
    let scanner =
      WorkspaceScanner.create(FakeChannel.with_stdout(h.env.out))
    let modules = recover ref scanner.scan(workspace).clone() end
    Sort[Array[ModuleData val], ModuleData val](modules)
    // debugging
    for module in modules.values() do
      h.log(module.string())
    end

    // utils
    let p1_path = workspace
      .join("_corral/github_com_ponylang_package1")?
    let p2_path = workspace
      .join("_corral/github_com_ponylang_package2")?

    // assert we found 3 modules
    h.assert_eq[USize](3, modules.size())

        // p1 depending on p2
    let p1 = modules.shift()?
    h.assert_eq[String]("p1", p1.name)
    h.assert_eq[String](
      p1_path.join("p1")?.path,
      p1.folder.path
    )
    AssertDependenciesEq(h, [p2_path], p1.dependencies)

    // p2 depending on p1
    let p2 = modules.shift()?
    h.assert_eq[String]("p2", p2.name)
    h.assert_eq[String](
      p2_path.join("p2")?.path,
      p2.folder.path
    )
    AssertDependenciesEq(h, [p1_path], p2.dependencies)

    // - cyclic  - with correct dependencies (p1, p2)
    let p0 = modules.shift()?
    h.assert_eq[String]("cyclic", p0.name)
    h.assert_eq[String](
      workspace.join("cyclic")?.path,
      p0.folder.path
    )
    AssertDependenciesEq(h, [p1_path; p2_path], p0.dependencies)


class \nodoc\ iso _WorkspaceWithBrokenCorral is WsUnitTest
  fun name(): String => "workspace/scanner/invalid_json"

  fun setup_workspace(ws: WorkspaceBuilder) =>
    ws.file("corral.json", """
      {
        "packages": ["broken"],
        "deps": [
          {
            "locator": "github.com/ponylang/package2.git",
            "version": "1.2.3"
          }
        ],
        "info": {
          "description": "ws-test-broken",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "ws-test-broken"
        }
    """)
    ws.dir("broken").file("main.pony", PonyMainSrc())
    ws.dir("_corral").file("main.pony", PonyMainSrc()) // should be ignored

  fun apply(workspace: FilePath, h: TestHelper)? =>
    let scanner =
      WorkspaceScanner.create(FakeChannel.with_stdout(h.env.out))
    let modules = recover ref scanner.scan(workspace).clone() end
    Sort[Array[ModuleData val], ModuleData val](modules)
    // debugging
    for module in modules.values() do
      h.log(module.string())
    end


    // assert we found 1 module
    h.assert_eq[USize](1, modules.size())
    let p0 = modules(0)?
    h.assert_eq[String]("broken", p0.name)
    h.assert_eq[String](
      workspace.join("broken")?.path,
      p0.folder.path
    )
    AssertDependenciesEq(h, [], p0.dependencies)

class \nodoc\ iso _WorkspaceWithEscapingCorralPackagePaths is WsUnitTest
  fun name(): String => "workspace/scanner/escaping_packages"

  fun setup_workspace(ws: WorkspaceBuilder) =>
    ws.file("corral.json", """
      {
        "packages": ["../../evil/escape"],
        "deps": [
          {
            "locator": "github.com/ponylang/package2.git",
            "version": "1.2.3"
          }
        ],
        "info": {
          "description": "ws-test-broken",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "ws-test-broken"
        }
      }
    """)
    ws.dir("_corral").dir("github_com_ponylang_package2").file("main.pony", PonyMainSrc()) // should be ignored

  fun apply(workspace: FilePath, h: TestHelper) =>
    let scanner =
      WorkspaceScanner.create(FakeChannel.with_stdout(h.env.out))
    let modules = recover ref scanner.scan(workspace).clone() end
    Sort[Array[ModuleData val], ModuleData val](modules)
    // debugging
    for module in modules.values() do
      h.log(module.string())
    end

    // assert we found no module
    h.assert_eq[USize](0, modules.size())


class \nodoc\ iso _WorkspaceWithoutCorral is WsUnitTest
  fun name(): String => "workspace/scanner/no_corral"
  fun setup_workspace(ws: WorkspaceBuilder) =>
    let d = ws.dir("scratch")
    d.file("main.pony", PonyMainSrc())
    d.dir("test").file("main.pony", PonyMainSrc())
    ws.dir("lib").file("foo.pony", "primitive _Foo")
    ws.dir("examples").dir("example_01").file("main.pony", PonyMainSrc())


  fun apply(workspace: FilePath, h: TestHelper) ? =>
    let scanner =
      WorkspaceScanner.create(FakeChannel.with_stdout(h.env.out))
    let modules = recover ref scanner.scan(workspace).clone() end
    Sort[Array[ModuleData val], ModuleData val](modules)
    // debugging
    for module in modules.values() do
      h.log(module.string())
    end

    // assert we found 4 modules
    h.assert_eq[USize](4, modules.size())

    let p0 = modules(0)?
    h.assert_eq[String]("examples/example_01", p0.name)
    h.assert_eq[String](
      workspace.join("examples/example_01")?.path,
      p0.folder.path
    )
    AssertDependenciesEq(h, [], p0.dependencies)

    let p1 = modules(1)?
    h.assert_eq[String]("lib", p1.name)
    h.assert_eq[String](
      workspace.join("lib")?.path,
      p1.folder.path
    )
    AssertDependenciesEq(h, [], p1.dependencies)

    let p2 = modules(2)?
    h.assert_eq[String]("scratch", p2.name)
    h.assert_eq[String](
      workspace.join("scratch")?.path,
      p2.folder.path
    )
    AssertDependenciesEq(h, [], p2.dependencies)

    let p3 = modules(3)?
    h.assert_eq[String]("scratch/test", p3.name)
    h.assert_eq[String](
      workspace.join("scratch/test")?.path,
      p3.folder.path
    )
    AssertDependenciesEq(h, [], p3.dependencies)

class \nodoc\ iso _WorkspaceWithoutCorralDir is WsUnitTest
  fun name(): String => "workspace/scanner/missing_corral_dir"

  fun setup_workspace(ws: WorkspaceBuilder) =>
    ws.file("corral.json", """
      {
        "packages": ["no_dir"],
        "deps": [
          {
            "locator": "github.com/ponylang/package1.git",
            "version": "0.6.2"
          }
        ],
        "info": {
          "description": "ws-test-no-dir",
          "homepage": "",
          "license": "",
          "documentation_url": "",
          "version": "0.2.0",
          "name": "ws-test"
        }
      }
    """)
    ws.dir("no_dir").file("main.pony", PonyMainSrc())

  fun apply(workspace: FilePath, h: TestHelper) ? =>
    let scanner =
      WorkspaceScanner.create(FakeChannel.with_stdout(h.env.out))
    let modules = recover ref scanner.scan(workspace).clone() end
    Sort[Array[ModuleData val], ModuleData val](modules)
    // debugging
    for module in modules.values() do
      h.log(module.string())
    end

    h.assert_eq[USize](1, modules.size())

    let p0 = modules.shift()?
    h.assert_eq[String]("no_dir", p0.name)
    h.assert_eq[String](
      workspace.join("no_dir")?.path,
      p0.folder.path
    )
    AssertDependenciesEq(h,
  [workspace.join("_corral/github_com_ponylang_package1")?], p0.dependencies)
