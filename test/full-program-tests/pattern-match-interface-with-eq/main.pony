// Regression test for `gen_pattern_eq` not handling vtable dispatch.
//
// When a `match` arm is a value whose static type is an interface or trait
// declaring `eq`, the pattern-matching machinery in `gen_pattern_eq`
// dispatches `eq` through the vtable. Before the fix, that site derived
// the call's function type via `LLVMGlobalGetValueType(func)`, but `func`
// for interface dispatch is a loaded pointer (opaque under LLVM's modern
// pointer model), not a global — so the returned type was wrong (or
// `NULL`), and ponyc itself crashed during LLVM IR generation.
//
// The fix uses `c_m->func_type` (the cached compile-method type) the same
// way `gen_call` does.

interface val Eq
  fun apply(): U64
  fun eq(other: Eq box): Bool => apply() == other.apply()

class val Box42
  fun apply(): U64 => 42
  fun eq(other: Eq box): Bool => apply() == other.apply()

actor Main
  new create(env: Env) =>
    let target: Eq = Box42
    let probe: Eq = Box42

    match target
    | probe => None
    else env.exitcode(1)
    end
