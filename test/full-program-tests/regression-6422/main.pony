"""
Short-circuit `and`/`or` must preserve side effects when the left operand
folds to a constant, and must produce valid IR when the left operand's
codegen creates multiple basic blocks.

See https://github.com/ponylang/ponyc/issues/6422
"""
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    var x: U32 = 1

    // `and` with constant true left: side effects must run.
    x = 1
    let r1 = ((x = x + 1; true) and (x > 2))
    if (x != 2) or (r1 != false) then
      @pony_exitcode(1)
      return
    end

    // `or` with constant false left: side effects must run.
    x = 1
    let r2 = ((x = x + 1; false) or (x > 2))
    if (x != 2) or (r2 != false) then
      @pony_exitcode(2)
      return
    end

    // `and` with constant false left: side effects must run.
    x = 1
    let r3 = ((x = x + 1; false) and (x > 2))
    if (x != 2) or (r3 != false) then
      @pony_exitcode(3)
      return
    end

    // `or` with constant true left: side effects must run.
    x = 1
    let r4 = ((x = x + 1; true) or (x > 2))
    if (x != 2) or (r4 != true) then
      @pony_exitcode(4)
      return
    end

    // Nested short-circuit that folds to a constant as a left operand must
    // not produce invalid IR.
    let y: U32 = 1
    let r5 = (((y > 3) or true) and (y > 0))
    if r5 != true then
      @pony_exitcode(5)
      return
    end

    let r6 = (((y > 3) and false) or (y > 0))
    if r6 != true then
      @pony_exitcode(6)
      return
    end

    // While loop with side effects in a constant-left short-circuit condition.
    var n: U32 = 0
    var i: U32 = 0
    while ((n = n + 1; true) and (i < 3)) do i = i + 1 end
    if n != 4 then
      @pony_exitcode(7)
      return
    end

    @pony_exitcode(0)
