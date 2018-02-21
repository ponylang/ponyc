use "ponytest"
use "promises"
//use "debug"
use "logger"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestSetGetBit)
    test(_TestDcApply)
    test(_TestReadLen)
    test(_TestConditionalPrint)
    test(_TestTmpBufTruncation)
    test(_TestTruncation)
    test(_TestWrapping)

class iso _TestSetGetBit is UnitTest
  fun name(): String => "DBG/TestSetGetBit"

  fun apply(h: TestHelper) =>
    let dc: DC = DC(2)
    h.assert_false(dc.gb(0))
    h.assert_false(dc.gb(1))
    dc.sb(0, true)
    h.assert_true(dc.gb(0))
    h.assert_false(dc.gb(1))
    dc.sb(1, true)
    h.assert_true(dc.gb(0))
    h.assert_true(dc.gb(1))
    dc.sb(0, false)
    h.assert_false(dc.gb(0))
    h.assert_true(dc.gb(1))
    dc.sb(1, false)
    h.assert_false(dc.gb(0))
    h.assert_false(dc.gb(1))

class iso _TestDcApply is UnitTest
  fun name(): String => "DBG/TestDcApply"

  fun apply(h: TestHelper) =>
    let dc: DC = DC(2)
    h.assert_false(dc(0))
    h.assert_false(dc(1))
    dc.sb(0, true)
    h.assert_true(dc(0))
    h.assert_false(dc(1))
    dc.sb(1, true)
    h.assert_true(dc(0))
    h.assert_true(dc(1))
    dc.sb(0, false)
    h.assert_false(dc(0))
    h.assert_true(dc(1))
    dc.sb(1, false)
    h.assert_false(dc(0))
    h.assert_false(dc(1))

class iso _TestReadLen is UnitTest
  fun name(): String => "DBG/TestReadLen"

  fun apply(h: TestHelper) =>
    let dc: DC = DC.create_with_dst_buf(0, 0x100)
    var buf: DbgReadBuf = DbgReadBuf(0x1000)
    dc.print("")
    h.assert_eq[USize](dc.read(buf), 0)

    dc.print("hi")
    h.assert_eq[USize](dc.read(buf), 2)

    dc.print("123")
    h.assert_eq[USize](dc.read(buf, 2), 2)
    //h.assert_eq[String ref](buf.string(), "12")
    h.assert_true(buf.string() == "12")
    h.assert_eq[USize](dc.read(buf), 1)
    //h.assert_eq[String ref](buf.string(), "3")
    h.assert_true(buf.string() == "3")

class iso _TestConditionalPrint is UnitTest
  fun name(): String => "DBG/TestConditionalPrint"

  fun apply(h: TestHelper) =>
    let dc: DC = DC.create_with_dst_buf(0, 0x100)
    var buf: DbgReadBuf = DbgReadBuf(0x1000)
    dc.sb(0, true)
    dc(0) and dc.print("bit 0: dc.print message, should print")
    dc(1) and dc.print("bit 1: dc.print message, shouldn't print")

    dc.read(buf)
    h.assert_true(buf.string() == "bit 0: dc.print message, should print")

class iso _TestTmpBufTruncation is UnitTest
  fun name(): String => "DBG/TestTmpBufTruncation"

  fun apply(h: TestHelper) =>
    let dc: DC = DC.create_with_dst_buf(0, 0x100, 0x10)
    var buf: DbgReadBuf = DbgReadBuf(0x1000)

    dc.print("0123456789ABCDEFtruncated")
    dc.read(buf)
    h.assert_true(buf.string() == "0123456789ABCDEF")

class iso _TestTruncation is UnitTest
  fun name(): String => "DBG/TestTruncation"

  fun apply(h: TestHelper) =>
    let dc: DC = DC.create_with_dst_buf(0, 0x10)
    var buf: DbgReadBuf = DbgReadBuf(0x1000)

    dc.print("0123456789ABCDEFtruncated")
    dc.read(buf)
    h.assert_true(buf.string() == "0123456789ABCDEF")

class iso _TestWrapping is UnitTest
  fun name(): String => "DBG/TestWrapping"

  fun apply(h: TestHelper) =>
    let dc: DC = DC.create_with_dst_buf(0, 0x10)
    var buf: DbgReadBuf = DbgReadBuf(0x1000)

    // Partially fill the buf
    dc.print("012456789")

    // Write more than can fit in buf
    dc.print("ABCDEFwrapped")

    // Read and verify the buffer wrapped
    dc.read(buf)
    h.assert_true(buf.string() == "789ABCDEFwrapped")

    // Test a second write that fills buffer truncating
    dc.print("0123456789ABCDEFtruncated")
    dc.read(buf)
    h.assert_true(buf.string() == "0123456789ABCDEF")
