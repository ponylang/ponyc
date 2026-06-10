#include <gtest/gtest.h>
#include <platform.h>
#include <type/alias.h>
#include <type/cap.h>
#include <ast/ast.h>

#include "util.h"


#define TEST_COMPILE(src) DO(test_compile(src, "expr"))


class AliasTest : public PassTest
{
protected:
  // Alias the arrow type of the named parameter and assert how its right side
  // was treated. short_circuit == true means alias() must return the input
  // node unchanged with an un-aliased right side (K->A); short_circuit ==
  // false means it must return a fresh arrow whose right side is aliased
  // (K->(A!)). Uses ASSERT, so wrap calls in DO().
  void check_arrow_alias(const char* name, bool short_circuit)
  {
    ast_t* type = type_of(name);
    // Guard the setup: a NULL or non-arrow type would otherwise make the
    // un-aliased assertion below pass vacuously.
    ASSERT_NE((void*)NULL, type);
    ASSERT_EQ(TK_ARROW, ast_id(type));

    ast_t* aliased = alias(type, &opt);
    token_id eph = ast_id(ast_sibling(cap_fetch(ast_childidx(aliased, 1))));

    if(short_circuit)
    {
      // The arrow is returned unchanged: the same node, right side unmarked.
      ASSERT_EQ(type, aliased);
      ASSERT_EQ(TK_NONE, eph);
    }
    else
    {
      // A fresh arrow whose right side carries the alias marker. Free it so the
      // unattached node doesn't leak.
      ASSERT_NE(type, aliased);
      ASSERT_EQ(TK_ALIASED, eph);
      ast_free_unattached(aliased);
    }
  }
};


// alias() of an arrow type K->A normally aliases the right side, producing
// K->(A!). For viewpoints that are self-aliasing for every instantiation of
// A, that is over-conservative: alias(K->A) should be K->A, so the arrow is
// returned unchanged. A val viewpoint yields val, tag, or #share, a box
// viewpoint those plus box, and a tag viewpoint only tag; all alias to
// themselves. iso, trn, and ref viewpoints can yield a non-self-aliasing cap
// (e.g. ref->iso = iso), so they must still alias the right side.
//
// The short-circuit is not observable through ordinary compilation: box->A
// and box->A! already compare as mutual subtypes (box is see-through, so both
// reify to equal concrete caps), and tag->A arrow types are independently
// blocked from binding/return by an unrelated ephemeral subtype check. These
// tests therefore guard the alias() behaviour directly at the source. See
// issue #4969 and the val precedent in #4963.

// Each arrow type is exposed via an interface method parameter (the canonical
// pattern from type_check_subtype.cc / subtype_cache.cc): every parameter has
// a unique name and a typed annotation, so type_of(name) returns one
// well-defined arrow AST per call.
static const char* TEST_SRC =
  "interface Test[A]\n"
  "  fun z(a_iso: iso->A, a_trn: trn->A, a_ref: ref->A,\n"
  "        a_val: val->A, a_box: box->A, a_tag: tag->A)";


// val, box, and tag viewpoints are self-aliasing, so alias() leaves the
// arrow's right side unmarked.
TEST_F(AliasTest, ArrowSelfAliasingViewpointShortCircuits)
{
  TEST_COMPILE(TEST_SRC);

  DO(check_arrow_alias("a_val", true));
  DO(check_arrow_alias("a_box", true));
  DO(check_arrow_alias("a_tag", true));
}


// iso, trn, and ref viewpoints can produce a non-self-aliasing cap, so alias()
// must still alias the right side. This is also the positive control for the
// test above: it confirms alias() does mark the right side when it should, so
// the unmarked results for val/box/tag are meaningful rather than vacuous.
TEST_F(AliasTest, ArrowNonSelfAliasingViewpointAliasesRight)
{
  TEST_COMPILE(TEST_SRC);

  DO(check_arrow_alias("a_iso", false));
  DO(check_arrow_alias("a_trn", false));
  DO(check_arrow_alias("a_ref", false));
}
