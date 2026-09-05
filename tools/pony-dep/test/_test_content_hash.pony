use "files"
use "pony_test"
use dep = ".."

class \nodoc\ _TestContentHashSingleFile is UnitTest
  fun name(): String => "ContentHash/single file"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let dir = Directory(tmp.path)?
    let f = dir.create_file("hello.pony")?
    f.write("actor")
    f.dispose()

    let root = dep.ContentHash(tmp.path)?
    h.assert_eq[USize](root.size(), 32)

    let expected = _ContentHashHelper._leaf("hello.pony", "actor")
    h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
    tmp.dispose()

class \nodoc\ _TestContentHashTwoFiles is UnitTest
  fun name(): String => "ContentHash/two files"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let dir = Directory(tmp.path)?
    let f1 = dir.create_file("b.pony")?
    f1.write("2")
    f1.dispose()
    let f2 = dir.create_file("a.pony")?
    f2.write("1")
    f2.dispose()

    let root = dep.ContentHash(tmp.path)?

    let leaf_a = _ContentHashHelper._leaf("a.pony", "1")
    let leaf_b = _ContentHashHelper._leaf("b.pony", "2")
    let expected = _ContentHashHelper._pair(leaf_a, leaf_b)
    h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
    tmp.dispose()

class \nodoc\ _TestContentHashThreeFiles is UnitTest
  fun name(): String => "ContentHash/three files (odd promotion)"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let dir = Directory(tmp.path)?
    let f1 = dir.create_file("c.pony")?
    f1.write("3")
    f1.dispose()
    let f2 = dir.create_file("a.pony")?
    f2.write("1")
    f2.dispose()
    let f3 = dir.create_file("b.pony")?
    f3.write("2")
    f3.dispose()

    let root = dep.ContentHash(tmp.path)?

    let leaf_a = _ContentHashHelper._leaf("a.pony", "1")
    let leaf_b = _ContentHashHelper._leaf("b.pony", "2")
    let leaf_c = _ContentHashHelper._leaf("c.pony", "3")

    let hash_ab = _ContentHashHelper._pair(leaf_a, leaf_b)
    let expected = _ContentHashHelper._pair(hash_ab, leaf_c)
    h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
    tmp.dispose()

class \nodoc\ _TestContentHashFourFiles is UnitTest
  fun name(): String => "ContentHash/four files (balanced)"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let dir = Directory(tmp.path)?
    let f1 = dir.create_file("d.pony")?
    f1.write("4")
    f1.dispose()
    let f2 = dir.create_file("b.pony")?
    f2.write("2")
    f2.dispose()
    let f3 = dir.create_file("c.pony")?
    f3.write("3")
    f3.dispose()
    let f4 = dir.create_file("a.pony")?
    f4.write("1")
    f4.dispose()

    let root = dep.ContentHash(tmp.path)?

    let leaf_a = _ContentHashHelper._leaf("a.pony", "1")
    let leaf_b = _ContentHashHelper._leaf("b.pony", "2")
    let leaf_c = _ContentHashHelper._leaf("c.pony", "3")
    let leaf_d = _ContentHashHelper._leaf("d.pony", "4")

    let hash_ab = _ContentHashHelper._pair(leaf_a, leaf_b)
    let hash_cd = _ContentHashHelper._pair(leaf_c, leaf_d)
    let expected = _ContentHashHelper._pair(hash_ab, hash_cd)
    h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
    tmp.dispose()

class \nodoc\ _TestContentHashFiveFiles is UnitTest
  fun name(): String => "ContentHash/five files (multi-level promotion)"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let dir = Directory(tmp.path)?
    let f1 = dir.create_file("e.pony")?
    f1.write("5")
    f1.dispose()
    let f2 = dir.create_file("c.pony")?
    f2.write("3")
    f2.dispose()
    let f3 = dir.create_file("a.pony")?
    f3.write("1")
    f3.dispose()
    let f4 = dir.create_file("d.pony")?
    f4.write("4")
    f4.dispose()
    let f5 = dir.create_file("b.pony")?
    f5.write("2")
    f5.dispose()

    let root = dep.ContentHash(tmp.path)?

    let leaf_a = _ContentHashHelper._leaf("a.pony", "1")
    let leaf_b = _ContentHashHelper._leaf("b.pony", "2")
    let leaf_c = _ContentHashHelper._leaf("c.pony", "3")
    let leaf_d = _ContentHashHelper._leaf("d.pony", "4")
    let leaf_e = _ContentHashHelper._leaf("e.pony", "5")

    // Level 0: [a, b, c, d, e] -> pair(a,b), pair(c,d), promote(e)
    let hash_ab = _ContentHashHelper._pair(leaf_a, leaf_b)
    let hash_cd = _ContentHashHelper._pair(leaf_c, leaf_d)
    // Level 1: [ab, cd, e] -> pair(ab,cd), promote(e)
    let hash_abcd = _ContentHashHelper._pair(hash_ab, hash_cd)
    // Level 2: [abcd, e] -> pair(abcd, e)
    let expected = _ContentHashHelper._pair(hash_abcd, leaf_e)
    h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
    tmp.dispose()

class \nodoc\ _TestContentHashPathBinding is UnitTest
  fun name(): String => "ContentHash/path binding prevents swap"

  fun apply(h: TestHelper) ? =>
    let tmp1 = _TestHelper.tmp_dir(h)?
    let dir1 = Directory(tmp1.path)?
    let f1a = dir1.create_file("a.pony")?
    f1a.write("X")
    f1a.dispose()
    let f1b = dir1.create_file("b.pony")?
    f1b.write("Y")
    f1b.dispose()

    let tmp2 = _TestHelper.tmp_dir(h)?
    let dir2 = Directory(tmp2.path)?
    let f2a = dir2.create_file("a.pony")?
    f2a.write("Y")
    f2a.dispose()
    let f2b = dir2.create_file("b.pony")?
    f2b.write("X")
    f2b.dispose()

    h.assert_ne[String](
      dep.Sha256.hex(dep.ContentHash(tmp1.path)?),
      dep.Sha256.hex(dep.ContentHash(tmp2.path)?))
    tmp1.dispose()
    tmp2.dispose()

class \nodoc\ _TestContentHashEmpty is UnitTest
  fun name(): String => "ContentHash/empty directory"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let root = dep.ContentHash(tmp.path)?
    h.assert_eq[USize](root.size(), 32)
    var all_zero = true
    var i: USize = 0
    while i < root.size() do
      try
        if root(i)? != 0 then all_zero = false end
      end
      i = i + 1
    end
    h.assert_true(all_zero)
    tmp.dispose()

class \nodoc\ _TestContentHashEmptyContent is UnitTest
  fun name(): String => "ContentHash/empty file content"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let dir = Directory(tmp.path)?
    let f = dir.create_file("empty.pony")?
    f.dispose()

    let root = dep.ContentHash(tmp.path)?

    let expected = _ContentHashHelper._leaf("empty.pony", "")
    h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
    tmp.dispose()

class \nodoc\ _TestContentHashNestedDirs is UnitTest
  fun name(): String => "ContentHash/nested directories"

  fun apply(h: TestHelper) ? =>
    let tmp = _TestHelper.tmp_dir(h)?
    let dir = Directory(tmp.path)?
    dir.mkdir("src")
    let f1 = dir.create_file("src/main.pony")?
    f1.write("actor Main")
    f1.dispose()
    let f2 = dir.create_file("README.md")?
    f2.write("hello")
    f2.dispose()

    let root = dep.ContentHash(tmp.path)?

    let leaf_readme = _ContentHashHelper._leaf("README.md", "hello")
    let leaf_main =
      _ContentHashHelper._leaf("src/main.pony", "actor Main")
    let expected = _ContentHashHelper._pair(leaf_readme, leaf_main)
    h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
    tmp.dispose()

class \nodoc\ _TestContentHashSkipsSymlinks is UnitTest
  fun name(): String => "ContentHash/skips symlinks"

  fun apply(h: TestHelper) ? =>
    ifdef not windows then
      let tmp = _TestHelper.tmp_dir(h)?
      let dir = Directory(tmp.path)?
      let f = dir.create_file("real.pony")?
      f.write("content")
      f.dispose()

      let source = tmp.path.join("real.pony")?
      dir.symlink(source, "link.pony")

      let root = dep.ContentHash(tmp.path)?

      let expected = _ContentHashHelper._leaf("real.pony", "content")
      h.assert_eq[String](dep.Sha256.hex(root), dep.Sha256.hex(expected))
      tmp.dispose()
    end

primitive \nodoc\ _ContentHashHelper
  fun _leaf(path: String val, content: String val): Array[U8] val =>
    dep.Sha256(
      recover val
        Array[U8](path.size() + 1 + content.size())
          .> append(path)
          .> push(0x00)
          .> append(content)
      end)

  fun _pair(left: Array[U8] val, right: Array[U8] val): Array[U8] val =>
    dep.Sha256(
      recover val
        Array[U8](64)
          .> append(left)
          .> append(right)
      end)
