// assert.pony provides a simple assertion
// facility that does not require
// threading pony_check's TestHelper
// through production code.
//
// We endeavor to exit(1) the program immediately
// upon a failed assertion.
//
// Note that even when using the C FFI, your
// assert error message may still
// not be the last thing printed in the output.
// Other threads may get to print before the
// process is terminated. Scroll up in your
// output, searching for "error:" if you
// don't see your assertion message at the
// end of the output.

use @printf[I32](fmt: Pointer[U8] tag, ...)
use @fprintf[I32](stream:Pointer[U8], fmt: Pointer[U8] tag, ...)
use @exit[None](status:I32)
use @pony_os_stdout[Pointer[U8]]()
use @pony_os_stderr[Pointer[U8]]()
use @fflush[I32](stream:Pointer[U8])
use @write[USize](fd:I32, buf:Pointer[U8] tag, sz:USize)

primitive Assert
  fun crash(msg:String, loc:SourceLoc = __loc) =>
      // try to get our own line for better visibility.
      @fflush[I32](@pony_os_stdout())
      @fflush[I32](@pony_os_stderr())
      let msg3 = "\n"+loc.file() + ":" + loc.line().string() + "\n" + loc.type_name() + "." + loc.method_name() + ": " + msg + "\n"
      @write(I32(2), msg3.cstring(), msg3.size())  // 2 = stderr
      @fflush[I32](@pony_os_stderr())
      @exit(1)

  fun apply(mustHold:Bool, invariantText:String, loc: SourceLoc = __loc) =>
    """
    Assert.apply asserts an invariant by allowing the caller to supply
    an expression that evaluates to a Bool, mustHold.
    Assert.apply crashes and reports a violated invariant if mustHold is false.
    Assert.apply() is morally equivalent to Assert.invar().
    """
    if mustHold then
      return
    end
    crash("error: Assert.apply invariant '" + invariantText + "' violated!", loc)

  fun invar(mustHold:Bool, invariantText:String, loc: SourceLoc = __loc) =>
    """
      Assert.invar crashes if mustHold is false. equivalent to Assert.apply().
    """
    if mustHold then
      return
    end
    crash("error: Assert.invar '" + invariantText + "' violated!", loc)
          
  fun equal[T: (Equatable[T] #read & Stringable #read)](got:T, want:T, inv:String = "", loc: SourceLoc = __loc) =>
    if got != want then
      crash("error: Assert.equal violated! want: " + want.string() + ", but got: " + got.string() + "; "+inv, loc)
    end 

  fun equalbox[T: (Equatable[T] box & Stringable box)](got:T, want:T, inv:String = "", loc: SourceLoc = __loc) =>
    if got != want then
      crash("error: Assert.equalbox violated! want: " + want.string() + ", but got: " + got.string() + "; "+inv, loc)
    end 
    
  fun lte[T: (Comparable[T] #read & Stringable #read)](a:T, b:T, inv:String, loc: SourceLoc = __loc) =>
    if a <= b then
      return
    end
    crash("error: Assert.lte violated! want: " + a.string() + " <= " + b.string() + " since '" + inv + "'", loc)

  fun gte[T: (Comparable[T] #read & Stringable #read)](a:T, b:T, inv:String, loc: SourceLoc = __loc) =>
    if a >= b then
      return
    end
    crash("error: Assert.gte violated! want: " + a.string() + " <= " + b.string() + " since '" + inv + "'", loc)

  fun lt[T: (Comparable[T] #read & Stringable #read)](a:T, b:T, inv:String, loc: SourceLoc = __loc) =>
    if a < b then
      return
    end
    crash("error: Assert.lt violated! want: " + a.string() + " < " + b.string() + " since '" + inv + "'")

  fun gt[T: (Comparable[T] #read & Stringable #read)](a:T, b:T, inv:String, loc: SourceLoc = __loc) =>
    if a > b then
      return
    end
    crash("error: Assert.gt violated! want: " + a.string() + " > " + b.string() + " since '" + inv + "'", loc)
    
