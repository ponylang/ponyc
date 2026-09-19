use "collections"
use "files"
use "pony_test"

use dep = ".."

primitive \nodoc\ _CleanTestHelper
  fun packages_path(h: TestHelper): String val =>
    """
    The first PONYPATH entry from the test environment, or empty string
    when unset.
    """
    match h.env.vars
    | let vars: Array[String val] val =>
      for pair in vars.values() do
        if pair.at("PONYPATH=") then
          return pair.substring(ISize(9))
        end
      end
    end
    ""

  fun write_pony_file(
    auth: FileAuth,
    dir: String,
    content: String)
    ?
  =>
    let fp = FilePath(auth, Path.join(dir, "main.pony"))
    let f = CreateFile(fp) as File
    f.print(content)
    f.dispose()

  fun make_dir(auth: FileAuth, path: String) ? =>
    let fp = FilePath(auth, path)
    if not fp.mkdir() then error end

class \nodoc\ _TestCleanEmptyExt is UnitTest
  """Clean with no directories in ext removes nothing."""

  fun name(): String => "Clean removes nothing from empty ext"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.write_pony_file(auth, src,
      "primitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](0, removed.size())
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanUnreferencedRemoved is UnitTest
  """An unreferenced directory in ext is removed."""

  fun name(): String => "Clean removes unreferenced ext directory"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let stale = Path.join(ext, "stale-pkg")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, stale)?
    _CleanTestHelper.write_pony_file(auth, stale,
      "primitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, src,
      "primitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](1, removed.size())
      try
        h.assert_eq[String val]("stale-pkg", removed(0)?)
      else
        h.fail("removed array access failed")
      end
      let stale_exists =
        try FileInfo(FilePath(auth, stale))?.directory
        else false
        end
      h.assert_false(stale_exists,
        "stale directory should have been deleted")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanReferencedKept is UnitTest
  """A directory referenced by src stays."""

  fun name(): String => "Clean keeps referenced ext directory"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let live = Path.join(ext, "live-pkg")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, live)?
    _CleanTestHelper.write_pony_file(auth, live,
      "primitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, src,
      "use \"ext:live-pkg\"\nprimitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](0, removed.size())
      h.assert_true(
        FileInfo(FilePath(auth, live))?.directory,
        "live directory should still exist")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanTransitiveKept is UnitTest
  """Transitive references keep both direct and indirect deps."""

  fun name(): String => "Clean keeps transitively referenced directories"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let pkg_a = Path.join(ext, "pkg-a")
    let pkg_b = Path.join(ext, "pkg-b")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, pkg_a)?
    _CleanTestHelper.make_dir(auth, pkg_b)?

    _CleanTestHelper.write_pony_file(auth, src,
      "use \"ext:pkg-a\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, pkg_a,
      "use \"ext:pkg-b\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, pkg_b,
      "primitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](0, removed.size())
      h.assert_true(
        FileInfo(FilePath(auth, pkg_a))?.directory,
        "pkg-a should still exist")
      h.assert_true(
        FileInfo(FilePath(auth, pkg_b))?.directory,
        "pkg-b should still exist")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanTransitiveWithUnreferenced is UnitTest
  """Transitive chain keeps A and B; unreferenced C is removed."""

  fun name(): String =>
    "Clean removes unreferenced alongside transitive chain"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let pkg_a = Path.join(ext, "pkg-a")
    let pkg_b = Path.join(ext, "pkg-b")
    let pkg_c = Path.join(ext, "pkg-c")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, pkg_a)?
    _CleanTestHelper.make_dir(auth, pkg_b)?
    _CleanTestHelper.make_dir(auth, pkg_c)?

    _CleanTestHelper.write_pony_file(auth, src,
      "use \"ext:pkg-a\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, pkg_a,
      "use \"ext:pkg-b\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, pkg_b,
      "primitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, pkg_c,
      "primitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](1, removed.size())
      h.assert_true(
        FileInfo(FilePath(auth, pkg_a))?.directory,
        "pkg-a should still exist")
      h.assert_true(
        FileInfo(FilePath(auth, pkg_b))?.directory,
        "pkg-b should still exist")
      let c_exists =
        try FileInfo(FilePath(auth, pkg_c))?.directory
        else false
        end
      h.assert_false(c_exists, "pkg-c should have been removed")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanMixed is UnitTest
  """Multiple directories: some referenced, some not."""

  fun name(): String => "Clean removes only unreferenced directories"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let live1 = Path.join(ext, "live1")
    let live2 = Path.join(ext, "live2")
    let dead1 = Path.join(ext, "dead1")
    let dead2 = Path.join(ext, "dead2")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, live1)?
    _CleanTestHelper.make_dir(auth, live2)?
    _CleanTestHelper.make_dir(auth, dead1)?
    _CleanTestHelper.make_dir(auth, dead2)?

    _CleanTestHelper.write_pony_file(auth, src,
      "use \"ext:live1\"\nuse \"ext:live2\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, live1,
      "primitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, live2,
      "primitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, dead1,
      "primitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, dead2,
      "primitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](2, removed.size())
      let removed_set = HashSet[String, HashEq[String]]
      for entry in removed.values() do removed_set.set(entry) end
      h.assert_true(removed_set.contains("dead1"))
      h.assert_true(removed_set.contains("dead2"))
      h.assert_true(
        FileInfo(FilePath(auth, live1))?.directory,
        "live1 should still exist")
      h.assert_true(
        FileInfo(FilePath(auth, live2))?.directory,
        "live2 should still exist")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanSourceNotFound is UnitTest
  fun name(): String => "Clean returns error for missing src directory"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let ext = Path.join(tmp.path.path, "ext")
    _CleanTestHelper.make_dir(auth, ext)?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(
      auth,
      Path.join(tmp.path.path, "no-such-src"),
      ext,
      pkg_path)
    | dep.CleanSourceNotFound => None
    else
      h.fail("expected CleanSourceNotFound")
    end
    tmp.dispose()

class \nodoc\ _TestCleanSourceNotDirectory is UnitTest
  fun name(): String =>
    "Clean returns error when src path is a file"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src_file = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    _CleanTestHelper.make_dir(auth, ext)?

    let fp = FilePath(auth, src_file)
    let f = CreateFile(fp) as File
    f.print("not a directory")
    f.dispose()

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src_file, ext, pkg_path)
    | dep.CleanSourceNotDirectory => None
    else
      h.fail("expected CleanSourceNotDirectory")
    end
    tmp.dispose()

class \nodoc\ _TestCleanExtNotFound is UnitTest
  fun name(): String => "Clean returns error for missing ext directory"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.write_pony_file(auth, src,
      "primitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(
      auth,
      src,
      Path.join(tmp.path.path, "no-such-ext"),
      pkg_path)
    | dep.CleanExtNotFound => None
    else
      h.fail("expected CleanExtNotFound")
    end
    tmp.dispose()

class \nodoc\ _TestCleanExtNotDirectory is UnitTest
  fun name(): String =>
    "Clean returns error when ext path is a file"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext_file = Path.join(tmp.path.path, "ext")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.write_pony_file(auth, src,
      "primitive Placeholder")?

    let fp = FilePath(auth, ext_file)
    let f = CreateFile(fp) as File
    f.print("not a directory")
    f.dispose()

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext_file, pkg_path)
    | dep.CleanExtNotDirectory => None
    else
      h.fail("expected CleanExtNotDirectory")
    end
    tmp.dispose()

class \nodoc\ _TestCleanUnparseableSourceFails is UnitTest
  """A source directory that fails to parse produces an error."""

  fun name(): String =>
    "Clean returns error when source fails to parse"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.write_pony_file(auth, src,
      "this is not valid pony")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let e: dep.CleanFailed =>
      h.assert_true(e.message.contains("cannot parse"))
    else
      h.fail("expected CleanFailed")
    end
    tmp.dispose()

class \nodoc\ _TestCleanUnparseablePackageStillReachable is UnitTest
  """A package that fails to parse is still kept if something references it."""

  fun name(): String =>
    "Clean keeps unparseable package when referenced"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let bad_pkg = Path.join(ext, "bad-pkg")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, bad_pkg)?

    _CleanTestHelper.write_pony_file(auth, src,
      "use \"ext:bad-pkg\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, bad_pkg,
      "this is not valid pony")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](0, removed.size())
      h.assert_true(
        FileInfo(FilePath(auth, bad_pkg))?.directory,
        "bad-pkg should still exist — it's referenced")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanCyclicRefsTerminate is UnitTest
  """Cyclic ext references terminate without hanging."""

  fun name(): String => "Clean terminates with cyclic ext references"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let pkg_a = Path.join(ext, "pkg-a")
    let pkg_b = Path.join(ext, "pkg-b")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, pkg_a)?
    _CleanTestHelper.make_dir(auth, pkg_b)?

    _CleanTestHelper.write_pony_file(auth, src,
      "use \"ext:pkg-a\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, pkg_a,
      "use \"ext:pkg-b\"\nprimitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, pkg_b,
      "use \"ext:pkg-a\"\nprimitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](0, removed.size())
      h.assert_true(
        FileInfo(FilePath(auth, pkg_a))?.directory,
        "pkg-a should still exist")
      h.assert_true(
        FileInfo(FilePath(auth, pkg_b))?.directory,
        "pkg-b should still exist")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanAliasedUseKept is UnitTest
  """An aliased use statement still keeps the referenced directory."""

  fun name(): String => "Clean keeps directory from aliased use statement"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    let pkg = Path.join(ext, "my-pkg")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.make_dir(auth, pkg)?
    _CleanTestHelper.write_pony_file(auth, pkg,
      "primitive Placeholder")?
    _CleanTestHelper.write_pony_file(auth, src,
      "use myalias = \"ext:my-pkg\"\nprimitive Placeholder")?

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](0, removed.size())
      h.assert_true(
        FileInfo(FilePath(auth, pkg))?.directory,
        "my-pkg should still exist")
    else
      h.fail("expected success")
    end
    tmp.dispose()

class \nodoc\ _TestCleanFilesInExtIgnored is UnitTest
  """Regular files in ext (not directories) are left alone."""

  fun name(): String => "Clean ignores non-directory entries in ext"

  fun apply(h: TestHelper) ? =>
    let auth = FileAuth(h.env.root)
    let tmp = _TestHelper.tmp_dir(h)?
    let src = Path.join(tmp.path.path, "src")
    let ext = Path.join(tmp.path.path, "ext")
    _CleanTestHelper.make_dir(auth, src)?
    _CleanTestHelper.make_dir(auth, ext)?
    _CleanTestHelper.write_pony_file(auth, src,
      "primitive Placeholder")?

    let stray_file = FilePath(auth, Path.join(ext, "stray.txt"))
    let f = CreateFile(stray_file) as File
    f.print("not a package")
    f.dispose()

    let pkg_path = _CleanTestHelper.packages_path(h)
    match dep.Clean(auth, src, ext, pkg_path)
    | let removed: Array[String val] val =>
      h.assert_eq[USize](0, removed.size())
      try
        FileInfo(stray_file)?
      else
        h.fail("stray.txt should still exist")
      end
    else
      h.fail("expected success")
    end
    tmp.dispose()
