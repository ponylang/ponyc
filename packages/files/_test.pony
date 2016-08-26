use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestMkdtemp)
    test(_TestWalk)
    test(_TestDirectoryOpen)
    test(_TestPathClean)
    test(_TestPathJoin)
    test(_TestPathRel)
    test(_TestPathSplit)
    test(_TestPathDir)
    test(_TestPathExt)
    test(_TestPathVolume)

primitive _FileHelper
  fun make_files(h: TestHelper, files: Array[String]): FilePath? =>
    let top = Directory(FilePath.mkdtemp(h.env.root as AmbientAuth,
      "tmp._FileHelper."))
    for f in files.values() do
      try
        // Since we embed paths, we use the posix separator, even on Windows.
        let dir_head = Path.split(f, "/")
        let fp = FilePath(top.path, dir_head._1)
        let r = fp.mkdir()
        if dir_head._2 != "" then
          Directory(fp).create_file(dir_head._2).dispose()
        end
      else
        h.fail("Failed to create file: " + f)
        h.assert_true(top.path.remove())
        error
      end
    end
    top.path


class iso _TestMkdtemp is UnitTest
  fun name(): String => "files/FilePath.mkdtemp"
  fun apply(h: TestHelper) ? =>
    let tmp = FilePath.mkdtemp(h.env.root as AmbientAuth, "tmp.TestMkdtemp.")
    try
      h.assert_true(FileInfo(tmp).directory)
    then
      h.assert_true(tmp.remove())
    end


class iso _TestWalk is UnitTest
  fun name(): String => "files/FilePath.walk"
  fun apply(h: TestHelper) ? =>
    let top = _FileHelper.make_files(
      h, ["a/1", "a/2", "b", "c/3", "c/4", "d/5", "d/6"])
    try
      top.walk(
        lambda(dir: FilePath, entries: Array[String] ref)(p = top.path, h) =>
          if dir.path == p then
            h.assert_array_eq_unordered[String](["b", "c", "a", "d"], entries)
          elseif dir.path.at("a", -1) then
            h.assert_array_eq_unordered[String](["1", "2"], entries)
          elseif dir.path.at("c", -1) then
            h.assert_array_eq_unordered[String](["3", "4"], entries)
          elseif dir.path.at("d", -1) then
            h.assert_array_eq_unordered[String](["5", "6"], entries)
          else
            h.fail("Unexpected dir: " + dir.path)
          end
        end)
    then
      h.assert_true(top.remove())
    end


class iso _TestDirectoryOpen is UnitTest
  fun name(): String => "files/File.open.directory"
  fun apply(h: TestHelper) ? =>
    let tmp = FilePath.mkdtemp(h.env.root as AmbientAuth, "tmp.TestDiropen.")

    try
      h.assert_true(FileInfo(tmp).directory)
      let file = File.open(tmp)
      h.assert_true(file.errno() is FileError)
      h.assert_false(file.valid())
    then
      h.assert_true(tmp.remove())
    end


class iso _TestPathClean is UnitTest
  fun name(): String => "files/Path.clean"
  fun apply(h: TestHelper) =>
    let res1 = Path.clean("//foo/bar///")
    let res2 = Path.clean("foo/./bar")
    let res3 = Path.clean("foo/../foo")
    let res4 = Path.clean("///foo///bar///base.ext")

    ifdef windows then
      h.assert_eq[String](res1, "\\foo\\bar")
      h.assert_eq[String](res2, "foo\\bar")
      h.assert_eq[String](res3, "foo")
      h.assert_eq[String](res4, "\\foo\\bar\\base.ext")
    else
      h.assert_eq[String](res1, "/foo/bar")
      h.assert_eq[String](res2, "foo/bar")
      h.assert_eq[String](res3, "foo")
      h.assert_eq[String](res4, "/foo/bar/base.ext")
    end


class iso _TestPathJoin is UnitTest
  fun name(): String => "files/Path.join"
  fun apply(h: TestHelper) =>
    let path1 = "//foo/bar///"
    let path2 = "foo/./bar"
    let path3 = "foo/../foo"
    let path4 = "///foo///dir///base.ext"

    let res1 = Path.join(path1, path2)
    let res2 = Path.join(res1, path3)
    let res3 = Path.join(res2, path4)

    ifdef windows then
      h.assert_eq[String](res1, "\\foo\\bar\\foo\\bar")
      h.assert_eq[String](res2, "\\foo\\bar\\foo\\bar\\foo")
      h.assert_eq[String](res3, "\\foo\\dir\\base.ext")
    else
      h.assert_eq[String](res1, "/foo/bar/foo/bar")
      h.assert_eq[String](res2, "/foo/bar/foo/bar/foo")
      h.assert_eq[String](res3, "/foo/dir/base.ext")
    end


class iso _TestPathRel is UnitTest
  fun name(): String => "files/Path.rel"
  fun apply(h: TestHelper) ? =>
    let res = Path.rel("foo/bar", "foo/bar/baz")
    h.assert_eq[String](res, "baz")


class iso _TestPathSplit is UnitTest
  fun name(): String => "files/Path.split"
  fun apply(h: TestHelper) =>
    var path = "/foo/bar/dir/"
    let expect = [
      ("/foo/bar/dir", ""),
      ("/foo/bar", "dir"),
      ("/foo", "bar"),
      (".", "foo"),
      ("", ".")
    ]

    for parts in expect.values() do
      let res = Path.split(path)
      h.assert_eq[String](res._1, parts._1)
      h.assert_eq[String](res._2, parts._2)
      path = parts._1
    end


class iso _TestPathDir is UnitTest
  fun name(): String => "files/Path.dir"
  fun apply(h: TestHelper) =>
    let res1 = Path.dir("/foo/bar/dir/base.ext")
    let res2 = Path.dir("/foo/bar/dir/")

    ifdef windows then
      h.assert_eq[String](res1, "\\foo\\bar\\dir")
      h.assert_eq[String](res2, "\\foo\\bar\\dir")
    else
      h.assert_eq[String](res1, "/foo/bar/dir")
      h.assert_eq[String](res2, "/foo/bar/dir")
    end


class iso _TestPathExt is UnitTest
  fun name(): String => "files/Path.ext"
  fun apply(h: TestHelper) =>
    let res1 = Path.ext("/foo/bar/dir/base.ext")
    let res2 = Path.ext("/foo/bar/dir/base.ext/")

    h.assert_eq[String](res1, "ext")
    h.assert_eq[String](res2, "")


class iso _TestPathVolume is UnitTest
  fun name(): String => "files/Path.volume"
  fun apply(h: TestHelper) =>
    let res1 = Path.volume("C:\\foo")
    let res2 = Path.volume("\\foo")

    ifdef windows then
      h.assert_eq[String](res1, "C:")
      h.assert_eq[String](res2, "")
    else
      h.assert_eq[String](res1, "")
      h.assert_eq[String](res2, "")
    end
