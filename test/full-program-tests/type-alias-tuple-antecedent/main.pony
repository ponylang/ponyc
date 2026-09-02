// Regression test for a memory leak in find_antecedent_type's TK_TUPLE
// arm when the antecedent type reaches a tuple through a type alias.
//
// find_tuple_type's TK_TYPEALIASREF arm unfolds the alias, dups the
// TK_TUPLETYPE it finds inside, and frees the unfolded tree. The dup
// root is the TK_TUPLETYPE itself. find_antecedent_type then returns a
// child of that dup, orphaning the root. The compiler runs
// expr_pre_array twice on the array literal (once as written, once
// after wrapping it in a recover), so the program below leaks two dup
// roots per compile.
//
// See ponylang/ponyc#5958.

use @pony_exitcode[None](code: I32)

type _Pair is (Array[U8] val, U8)

actor Main
  new create(env: Env) =>
    let p: _Pair = ([1; 2], 3)
    @pony_exitcode(1)
