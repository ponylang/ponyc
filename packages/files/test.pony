use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestMkdtemp)
    test(_TestWalk)

primitive _FileHelper
  fun make_files(h: TestHelper, files: Array[String]): FilePath? =>
    let top = Directory(FilePath.mkdtemp(h.env.root, "tmp._FileHelper."))
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
        h.assert_failed("Failed to create file: " + f)
        h.expect_true(top.path.remove())
        error
      end
    end
    top.path


class iso _TestMkdtemp is UnitTest
  fun name(): String => "files/FilePath.mkdtemp"
  fun apply(h: TestHelper): TestResult? =>
    let tmp = FilePath.mkdtemp(h.env.root, "tmp.TestMkdtemp.")
    try
      h.expect_true(FileInfo(tmp).directory)
    then
      h.expect_true(tmp.remove())
    end
    true

class iso _TestWalk is UnitTest
  fun name(): String => "files/FilePath.walk"
  fun apply(h: TestHelper): TestResult? =>
    let top = _FileHelper.make_files(
      h, ["a/1", "a/2", "b", "c/3", "c/4", "d/5", "d/6"])
    try
      top.walk(
        lambda(dir: FilePath, entries: Array[String] ref)(p = top.path, h) =>
          if dir.path == p then
            h.expect_array_eq_unordered[String](["b", "c", "a", "d"], entries)
          elseif dir.path.at("a", -1) then
            h.expect_array_eq_unordered[String](["1", "2"], entries)
          elseif dir.path.at("c", -1) then
            h.expect_array_eq_unordered[String](["3", "4"], entries)
          elseif dir.path.at("d", -1) then
            h.expect_array_eq_unordered[String](["5", "6"], entries)
          else
            h.assert_failed("Unexpected dir: " + dir.path)
          end
        end)
    then
      h.expect_true(top.remove())
    end
    true
