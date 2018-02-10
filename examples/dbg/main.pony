use "dbg"
use "logger"

actor Main
  new create(env: Env) =>
    env.out.print("Yo from exmaples/dbg/main.pony")

    let dc: DC = DC.create_with_OutStream(1, env.out)

    // Use DC.appy with logger for finer grained logging
    let dc0: DC = DC(2)
    let logger = StringLogger(Fine, env.out)

    dc0(0) and logger.log("bit 0: logger.log message")
    dc0(1) and logger.log("bit 1: logger.log message, shouldn't print")


    // Create a DC with 2 bits and uses env.out as its dst
    let dc1: DC = DC.create_with_OutStream(2, env.out)

    // Print initialize values, which is false
    dc1.print("dc1.gb(0)=" + dc1.gb(0).string())
    dc1.print("dc1.gb(1)=" + dc1.gb(1).string())

    // Set bit 0 to true
    dc1.sb(0, true)

    // Read the values 0 = true and 1 is still false
    dc1.print("dc1.gb(0)=" + dc1.gb(0).string())
    dc1.print("dc1.gb(1)=" + dc1.gb(1).string())

    // Use DC.apply as a conditional in the same way as logger
    dc1(0) and dc1.print("bit 0: dc1.print message")
    dc1(1) and dc1.print("bit 1: dc1.print message, shouldn't print")

    // Create a DC which uses a buffer for its dst
    let dc2: DC = DC.create_with_dst_buf(2, 0x1000, 0x200)
    var buf2: DbgReadBuf = DbgReadBuf(0x1000)

    dc2.print("hi")
    var len = dc2.read(buf2)
    dc.print("dc.print buf len=" + len.string() + " s=" + buf2.string())

    // Create another DC with a buffer length of 16 and do partial reads
    let dc3: DC = DC.create_with_dst_buf(2, 0x10, 0x200)
    var buf3: DbgReadBuf = DbgReadBuf(0x1000)

    // Read 4 bytes, "Wink"
    dc3.print("Wink Saville")
    dc3.read(buf3, 4)
    dc.print("buf3(4)=\"" + buf3.string() + "\"")

    // Read the next byte, which is a space
    dc3.read(buf3, 1)
    dc.print("buf3(1)=\"" + buf3.string() + "\"")

    // Read the rest, "Saville"
    dc3.read(buf3)
    dc.print("buf=\"" + buf3.string() + "\"")

    // Write a long message to demonstrate the buffer truncates long strings
    // keeping the N first characters where N is the length of the buffer.
    dc3.print("0123456789ABCDEFtruncated")
    dc3.read(buf3)
    dc.print("buf=\"" + buf3.string() + "\"")

    // Additional writes are circular
    dc3.print("0123456789")
    dc3.print("ABCDEFwrapped")
    dc3.read(buf3)
    dc.print("buf=\"" + buf3.string() + "\"")

    // But again, we are always keeping the first N characters.
    dc3.print("0123456789ABCDEFtruncated")
    dc3.read(buf3)
    dc.print("buf=\"" + buf3.string() + "\"")

    let dc4: DC = DC.create_with_dst_buf(2, 0x100, 0x10)
    var buf4: DbgReadBuf = DbgReadBuf(0x1000)

    dc4.print("0123456789ABCDEFtruncated")
    dc4.read(buf4)
    dc.print("buf=\"" + buf4.string() + "\"")
