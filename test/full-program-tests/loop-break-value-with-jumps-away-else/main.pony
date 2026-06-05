use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // From the issue #5407 family. A break carrying a value gives the loop its
    // value and an exit even when the else clause jumps away, for both repeat
    // and while. This used to crash the compiler; verify the loop actually
    // yields the break value at runtime (and that the enclosing try sees it as a
    // success, not the error path), not just that it compiles.
    let r = try repeat break U8(3) until false else error end else U8(0) end
    let w = try while true do break U8(4) else error end else U8(0) end

    // A loop that genuinely jumps away (body always errors, no break) produces
    // no value, so the enclosing try falls back to its own else (0). The loop's
    // own else clause (2) is dead and must not become the value.
    let c = try repeat error until false else U8(2) end else U8(0) end

    if (r == 3) and (w == 4) and (c == 0) then
      @pony_exitcode(42)
    end
