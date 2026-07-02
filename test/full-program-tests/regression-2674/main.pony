"""
Regression test for https://github.com/ponylang/ponyc/issues/2674

A type parameter constrained by a union type where each member's cap is a
subset of #alias was being given cap #any. That made `_take(o)` style
re-aliasing fail with "argument type is O #any !" because #any can't be
re-aliased. The cap should be #alias.

Each primitive below exercises a different switch arm in
`cap_union_constraint`. If the bug returns for any pair, the corresponding
primitive's body fails to type-check and the test fails to build. The
primitives are declared but not all reified — body type-checking happens at
declaration time regardless of reification.

Naming convention: `_<CapA><CapB>` names the first and second cap in the
union's source order. `cap_from_constraint` left-folds with
`cap_union_constraint(a, b)`, so the source `(X capA | Y capB)` exercises the
switch arm for `capA` with subcase `capB`. The symmetric switch arm is a
separate code path and gets its own helper.
"""

interface tag _Async
  be write(data: ByteSeq)

interface ref _Sync
  fun ref apply()

interface box _Const
  fun apply()

// TK_REF switch arm.
primitive _RefTag[O: (_Sync ref | _Async tag)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _RefShare[O: (_Sync ref | Any #share)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _RefAlias[O: (_Sync ref | Any #alias)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// TK_VAL switch arm (the new TK_CAP_ALIAS entry is the only one this fix adds
// here; the pre-fix code also lacked a closing `break;` after TK_VAL, which
// the fix adds for structural correctness).
primitive _ValAlias[O: (_Const val | Any #alias)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// TK_BOX switch arm.
primitive _BoxTag[O: (_Const box | _Async tag)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _BoxShare[O: (_Const box | Any #share)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _BoxAlias[O: (_Const box | Any #alias)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// TK_TAG switch arm.
primitive _TagRef[O: (_Async tag | _Sync ref)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _TagBox[O: (_Async tag | _Const box)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _TagRead[O: (_Async tag | _Const #read)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _TagAlias[O: (_Async tag | Any #alias)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// TK_CAP_READ switch arm.
primitive _ReadTag[O: (_Const #read | _Async tag)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _ReadShare[O: (_Const #read | Any #share)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _ReadAlias[O: (_Const #read | Any #alias)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// TK_CAP_SHARE switch arm.
primitive _ShareRef[O: (Any #share | _Sync ref)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _ShareBox[O: (Any #share | _Const box)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _ShareRead[O: (Any #share | _Const #read)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _ShareAlias[O: (Any #share | Any #alias)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// TK_CAP_ALIAS switch arm. The pre-fix code lumped {ref, val, box, tag,
// cap_read} together with a single result; this fix adds cap_share to the
// same combined branch. Helpers for the pre-existing cases are included so a
// future regression that removes any one of them is caught.
primitive _AliasRef[O: (Any #alias | _Sync ref)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _AliasVal[O: (Any #alias | _Const val)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _AliasBox[O: (Any #alias | _Const box)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _AliasTag[O: (Any #alias | _Async tag)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _AliasRead[O: (Any #alias | _Const #read)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

primitive _AliasShare[O: (Any #alias | Any #share)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// Three-member fold chain. `cap_from_constraint` left-folds across the union,
// so this exercises `cap_union_constraint(#alias, _)` where the first
// argument was derived from a previous union step, not from a literal #alias
// member.
primitive _FoldChain[O: (_Sync ref | _Async tag | _Const box)]
  fun take(o: O) => _take(o)
  fun _take(o: O) => None

// The exact pattern from the issue, exercising the iftype-then-recall flow.
primitive _IssueExample[O: (_Async tag | _Sync ref)]
  fun _write(out: O, data: String) =>
    iftype O <: _Async tag then None
    elseif O <: _Sync ref then None
    end

  fun ok(out: O) =>
    _write(out, "ok")

actor Main
  new create(env: Env) =>
    _IssueExample[_Async].ok(env.out)
