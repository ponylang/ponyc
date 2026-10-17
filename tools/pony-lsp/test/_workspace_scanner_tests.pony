use "collections"
use "files"
use "itertools"
use "pony_test"
use "../workspace"

primitive _WorkspaceScannerTests is TestList
  new make() =>
    None

  fun tag tests(test: PonyTest) =>
    test(WsTest(_WorkspaceWithCorralJsonTest))
    test(WsTest(_WorkspaceWithCorralAndTransitiveDeps))

class \nodoc\ iso _WorkspaceWithCorralJsonTest is WsUnitTest
  fun name(): String => "workspace/scanner/corral"

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
    let scanner = WorkspaceScanner.create(FakeChannel)
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
        AssertDependenciesEq(h, package.dependencies, [p1_locator_path])
      elseif package.name == "package1" then
        h.assert_eq[String](package.folder.path, p1_path)
        AssertDependenciesEq(h, package.dependencies, [p1_locator_path])
      end
    end

primitive AssertDependenciesEq
  fun apply(h: TestHelper, expected: Array[FilePath] box, actual: Array[FilePath] box) =>
    h.assert_eq[USize](
      expected.size(),
      actual.size(),
      "Expected " + expected.size().string() + " elements, got " + actual.size().string()
    )
    for (idx, (expect, act)) in Iter[FilePath](expected.values()).zip[FilePath](actual.values()).enum[USize]() do
      h.assert_eq[String](expect.path, act.path, "Dependency " + idx.string() + " differs")
    end



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
    let packages = scanner.scan(workspace).clone()
    Sort[Array[PackageData val], PackageData val](packages)

    // utils
    let p1_path = workspace
      .join("_corral/github_com_ponylang_package1")?
    let p2_path = workspace
      .join("_corral/github_com_ponylang_package2")?
    let p3_path = workspace
      .join("_corral/gitlab_com_ponyslang_package3")?

    // assert we found 4 packages
    h.assert_eq[USize](4, packages.size())

    // - foo  - with correct dependencies (p1, p2, p3)
    let p0 = packages(0)?
    h.assert_eq[String]("foo", p0.name)
    AssertDependenciesEq(h, p0.dependencies, [p1_path; p2_path; p3_path])

    // - p1   - without dependencies ?
    let p1 = packages(1)?
    h.assert_eq[String]("p1", p1.name)

    // - p2   - with p3 dependency
    let p2 = packages(2)?
    h.assert_eq[String]("p2", p2.name)

    // - p3   - wothout dependencies ?
    let p3 = packages(3)?
    h.assert_eq[String]("p3", p3.name)


