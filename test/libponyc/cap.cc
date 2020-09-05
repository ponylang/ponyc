#include <gtest/gtest.h>
#include <platform.h>

#include <type/cap.h>
#include <ast/token.h>

#ifndef FOREACH_CAP
#define FOREACH_CAP(cap_param, constraint) \
token_id cap_param ## __array[6]; \
int cap_param ## __length = reify_cap_bounds(constraint, cap_param ## __array); \
int cap_param ## __i = 0; \
token_id cap_param; \
while(cap_param ## __i < cap_param ## __length) \
  if(1) {\
    cap_param = cap_param ## __array[cap_param ## __i]; \
    cap_param ## __i ++; \
    goto cap_param ## __label; \
  } else \
    cap_param ## __label :

#endif

class CapTest : public testing::Test
{
protected:
  static const token_id iso = TK_ISO;
  static const token_id trn = TK_TRN;
  static const token_id ref = TK_REF;
  static const token_id val = TK_VAL;
  static const token_id box = TK_BOX;
  static const token_id tag = TK_TAG;

  static const token_id read = TK_CAP_READ;
  static const token_id send = TK_CAP_SEND;
  static const token_id share = TK_CAP_SHARE;
  static const token_id alias = TK_CAP_ALIAS;
  static const token_id any = TK_CAP_ANY;

  static const token_id none = TK_NONE;
  static const token_id hat = TK_EPHEMERAL;

  int reify_cap_bounds(token_id cap, token_id dest_cap[6])
  {
    for (int i = 0; i < 6; i++) {
      dest_cap[i] = TK_NONE;
    }
    switch (cap) {
      case TK_ISO:
      case TK_TRN:
      case TK_REF:
      case TK_VAL:
      case TK_BOX:
      case TK_TAG:
        dest_cap[0] = cap;
        return 1;

      case TK_CAP_SEND:
        dest_cap[0] = TK_ISO;
        dest_cap[1] = TK_VAL;
        dest_cap[2] = TK_TAG;
        return 3;

      case TK_CAP_SHARE:
        dest_cap[0] = TK_VAL;
        dest_cap[1] = TK_TAG;
        return 2;

      case TK_CAP_READ:
        dest_cap[0] = TK_REF;
        dest_cap[1] = TK_VAL;
        dest_cap[2] = TK_BOX;
        return 3;

      case TK_CAP_ALIAS:
        dest_cap[0] = TK_REF;
        dest_cap[1] = TK_VAL;
        dest_cap[2] = TK_BOX;
        dest_cap[3] = TK_TAG;
        return 4;

      case TK_CAP_ANY:
        dest_cap[0] = TK_ISO;
        dest_cap[1] = TK_TRN;
        dest_cap[2] = TK_REF;
        dest_cap[3] = TK_VAL;
        dest_cap[4] = TK_BOX;
        dest_cap[5] = TK_TAG;
        return 6;

      default:
        return 0;
    }
  }

  bool is_sub(token_id sub, token_id sub_eph, token_id super, token_id sup_eph)
  {
    return is_cap_sub_cap(sub, sub_eph, super, sup_eph);
  }

  bool is_sub_constraint(token_id sub, token_id super)
  {
    return is_cap_sub_cap_constraint(sub, none, super, none);
  }

  bool is_sub_bound(token_id sub, token_id sub_eph, token_id super, token_id sup_eph)
  {
    return is_cap_sub_cap_bound(sub, sub_eph, super, sup_eph);
  }

  bool is_compat(token_id left, token_id right)
  {
    return is_cap_compat_cap(left, none, right, none);
  }

  token_id lower_viewpoint(token_id left, token_id right)
  {
    token_id eph = none;
    cap_view_lower(left, none, &right, &eph);
    return right;
  }

  token_id upper_viewpoint(token_id left, token_id right)
  {
    token_id eph = none;
    cap_view_upper(left, none, &right, &eph);
    return right;
  }
};

const token_id CapTest::iso;
const token_id CapTest::trn;
const token_id CapTest::ref;
const token_id CapTest::val;
const token_id CapTest::box;
const token_id CapTest::tag;

const token_id CapTest::read;
const token_id CapTest::send;
const token_id CapTest::share;
const token_id CapTest::alias;
const token_id CapTest::any;

const token_id CapTest::none;

TEST_F(CapTest, SubChecksConstraints)
{
  // for capsets K1, K2
  // K1 <= K2 if every instantation of K1 <= every instantation of K2
  token_id caps[] = { iso, trn, ref, val, box, tag, read, send, share, alias, any};
  int num_caps = sizeof(caps) / sizeof(token_id);
  for (int i = 0; i < num_caps; i++) {
    for (int j = 0; j < num_caps; j++) {
      token_id subcap = caps[i];
      token_id supcap = caps[j];

      bool expected = true;
      FOREACH_CAP(inst_subcap, subcap)
      {
        FOREACH_CAP(inst_supcap, supcap)
        {
          expected &= is_sub(inst_subcap, none, inst_supcap, none);
        }
      }
      bool actual = is_sub(subcap, none, supcap, none);
      ASSERT_EQ(expected, actual) <<
        "is_cap_sub_cap(" <<
        token_id_desc(subcap) << ", " <<
        token_id_desc(supcap) << ") was " <<
        actual << " but expected " <<
        expected << " based on manual instantiation";
    }
  }
}
TEST_F(CapTest, SubConcrete)
{
  // Every possible instantiation of sub must be a subtype of every possible
  // instantiation of super.

  // iso
  EXPECT_TRUE(is_sub(iso, hat, iso, hat));
  EXPECT_TRUE(is_sub(iso, hat, iso, none));
  EXPECT_TRUE(is_sub(iso, hat, trn, hat));
  EXPECT_TRUE(is_sub(iso, hat, trn, none));
  EXPECT_TRUE(is_sub(iso, hat, ref, none));
  EXPECT_TRUE(is_sub(iso, hat, val, none));
  EXPECT_TRUE(is_sub(iso, hat, box, none));
  EXPECT_TRUE(is_sub(iso, hat, tag, none));

  // trn
  EXPECT_FALSE(is_sub(trn, hat, iso, hat));
  EXPECT_FALSE(is_sub(trn, hat, iso, none));
  EXPECT_TRUE(is_sub(trn, hat, trn, hat));
  EXPECT_TRUE(is_sub(trn, hat, trn, none));
  EXPECT_TRUE(is_sub(trn, hat, ref, none));
  EXPECT_TRUE(is_sub(trn, hat, val, none));
  EXPECT_TRUE(is_sub(trn, hat, box, none));
  EXPECT_TRUE(is_sub(trn, hat, tag, none));

  // ref
  EXPECT_FALSE(is_sub(ref, none, iso, hat));
  EXPECT_FALSE(is_sub(ref, none, iso, none));
  EXPECT_FALSE(is_sub(ref, none, trn, hat));
  EXPECT_FALSE(is_sub(ref, none, trn, none));
  EXPECT_TRUE(is_sub(ref, none, ref, none));
  EXPECT_FALSE(is_sub(ref, none, val, none));
  EXPECT_TRUE(is_sub(ref, none, box, none));
  EXPECT_TRUE(is_sub(ref, none, tag, none));

  // val
  EXPECT_FALSE(is_sub(val, none, iso, hat));
  EXPECT_FALSE(is_sub(val, none, iso, none));
  EXPECT_FALSE(is_sub(val, none, trn, hat));
  EXPECT_FALSE(is_sub(val, none, trn, none));
  EXPECT_FALSE(is_sub(val, none, ref, none));
  EXPECT_TRUE(is_sub(val, none, val, none));
  EXPECT_TRUE(is_sub(val, none, box, none));
  EXPECT_TRUE(is_sub(val, none, tag, none));

  // box
  EXPECT_FALSE(is_sub(box, none, iso, hat));
  EXPECT_FALSE(is_sub(box, none, iso, none));
  EXPECT_FALSE(is_sub(box, none, trn, hat));
  EXPECT_FALSE(is_sub(box, none, trn, none));
  EXPECT_FALSE(is_sub(box, none, ref, none));
  EXPECT_FALSE(is_sub(box, none, val, none));
  EXPECT_TRUE(is_sub(box, none, box, none));
  EXPECT_TRUE(is_sub(box, none, tag, none));

  // tag
  EXPECT_FALSE(is_sub(tag, none, iso, hat));
  EXPECT_FALSE(is_sub(tag, none, iso, none));
  EXPECT_FALSE(is_sub(tag, none, trn, hat));
  EXPECT_FALSE(is_sub(tag, none, trn, none));
  EXPECT_FALSE(is_sub(tag, none, ref, none));
  EXPECT_FALSE(is_sub(tag, none, val, none));
  EXPECT_FALSE(is_sub(tag, none, box, none));
  EXPECT_TRUE(is_sub(tag, none, tag, none));
}

TEST_F(CapTest, SubConstraint)
{
  // Every possible instantiation of sub must be a member of the set of every
  // possible instantiation of super.

  // iso
  EXPECT_TRUE(is_sub_constraint(iso, iso));
  EXPECT_FALSE(is_sub_constraint(iso, trn));
  EXPECT_FALSE(is_sub_constraint(iso, ref));
  EXPECT_FALSE(is_sub_constraint(iso, val));
  EXPECT_FALSE(is_sub_constraint(iso, box));
  EXPECT_FALSE(is_sub_constraint(iso, tag));
  EXPECT_FALSE(is_sub_constraint(iso, read));
  EXPECT_TRUE(is_sub_constraint(iso, send));
  EXPECT_FALSE(is_sub_constraint(iso, share));
  EXPECT_FALSE(is_sub_constraint(iso, alias));
  EXPECT_TRUE(is_sub_constraint(iso, any));

  // trn
  EXPECT_FALSE(is_sub_constraint(trn, iso));
  EXPECT_TRUE(is_sub_constraint(trn, trn));
  EXPECT_FALSE(is_sub_constraint(trn, ref));
  EXPECT_FALSE(is_sub_constraint(trn, val));
  EXPECT_FALSE(is_sub_constraint(trn, box));
  EXPECT_FALSE(is_sub_constraint(trn, tag));
  EXPECT_FALSE(is_sub_constraint(trn, read));
  EXPECT_FALSE(is_sub_constraint(trn, send));
  EXPECT_FALSE(is_sub_constraint(trn, share));
  EXPECT_FALSE(is_sub_constraint(trn, alias));
  EXPECT_TRUE(is_sub_constraint(trn, any));

  // ref
  EXPECT_FALSE(is_sub_constraint(ref, iso));
  EXPECT_FALSE(is_sub_constraint(ref, trn));
  EXPECT_TRUE(is_sub_constraint(ref, ref));
  EXPECT_FALSE(is_sub_constraint(ref, val));
  EXPECT_FALSE(is_sub_constraint(ref, box));
  EXPECT_FALSE(is_sub_constraint(ref, tag));
  EXPECT_TRUE(is_sub_constraint(ref, read));
  EXPECT_FALSE(is_sub_constraint(ref, send));
  EXPECT_FALSE(is_sub_constraint(ref, share));
  EXPECT_TRUE(is_sub_constraint(ref, alias));
  EXPECT_TRUE(is_sub_constraint(ref, any));

  // val
  EXPECT_FALSE(is_sub_constraint(val, iso));
  EXPECT_FALSE(is_sub_constraint(val, trn));
  EXPECT_FALSE(is_sub_constraint(val, ref));
  EXPECT_TRUE(is_sub_constraint(val, val));
  EXPECT_FALSE(is_sub_constraint(val, box));
  EXPECT_FALSE(is_sub_constraint(val, tag));
  EXPECT_TRUE(is_sub_constraint(val, read));
  EXPECT_TRUE(is_sub_constraint(val, send));
  EXPECT_TRUE(is_sub_constraint(val, share));
  EXPECT_TRUE(is_sub_constraint(val, alias));
  EXPECT_TRUE(is_sub_constraint(val, any));

  // box
  EXPECT_FALSE(is_sub_constraint(box, iso));
  EXPECT_FALSE(is_sub_constraint(box, trn));
  EXPECT_FALSE(is_sub_constraint(box, ref));
  EXPECT_FALSE(is_sub_constraint(box, val));
  EXPECT_TRUE(is_sub_constraint(box, box));
  EXPECT_FALSE(is_sub_constraint(box, tag));
  EXPECT_TRUE(is_sub_constraint(box, read));
  EXPECT_FALSE(is_sub_constraint(box, send));
  EXPECT_FALSE(is_sub_constraint(box, share));
  EXPECT_TRUE(is_sub_constraint(box, alias));
  EXPECT_TRUE(is_sub_constraint(box, any));

  // tag
  EXPECT_FALSE(is_sub_constraint(tag, iso));
  EXPECT_FALSE(is_sub_constraint(tag, trn));
  EXPECT_FALSE(is_sub_constraint(tag, ref));
  EXPECT_FALSE(is_sub_constraint(tag, val));
  EXPECT_FALSE(is_sub_constraint(tag, box));
  EXPECT_TRUE(is_sub_constraint(tag, tag));
  EXPECT_FALSE(is_sub_constraint(tag, read));
  EXPECT_TRUE(is_sub_constraint(tag, send));
  EXPECT_TRUE(is_sub_constraint(tag, share));
  EXPECT_TRUE(is_sub_constraint(tag, alias));
  EXPECT_TRUE(is_sub_constraint(tag, any));

  // #read {ref, val, box}
  EXPECT_FALSE(is_sub_constraint(read, iso));
  EXPECT_FALSE(is_sub_constraint(read, trn));
  EXPECT_FALSE(is_sub_constraint(read, ref));
  EXPECT_FALSE(is_sub_constraint(read, val));
  EXPECT_FALSE(is_sub_constraint(read, box));
  EXPECT_FALSE(is_sub_constraint(read, tag));
  EXPECT_TRUE(is_sub_constraint(read, read));
  EXPECT_FALSE(is_sub_constraint(read, send));
  EXPECT_FALSE(is_sub_constraint(read, share));
  EXPECT_TRUE(is_sub_constraint(read, alias));
  EXPECT_TRUE(is_sub_constraint(read, any));

  // #send {iso, val, tag}
  EXPECT_FALSE(is_sub_constraint(send, iso));
  EXPECT_FALSE(is_sub_constraint(send, trn));
  EXPECT_FALSE(is_sub_constraint(send, ref));
  EXPECT_FALSE(is_sub_constraint(send, val));
  EXPECT_FALSE(is_sub_constraint(send, box));
  EXPECT_FALSE(is_sub_constraint(send, tag));
  EXPECT_FALSE(is_sub_constraint(send, read));
  EXPECT_TRUE(is_sub_constraint(send, send));
  EXPECT_FALSE(is_sub_constraint(send, share));
  EXPECT_FALSE(is_sub_constraint(send, alias));
  EXPECT_TRUE(is_sub_constraint(send, any));

  // #share {val, tag}
  EXPECT_FALSE(is_sub_constraint(share, iso));
  EXPECT_FALSE(is_sub_constraint(share, trn));
  EXPECT_FALSE(is_sub_constraint(share, ref));
  EXPECT_FALSE(is_sub_constraint(share, val));
  EXPECT_FALSE(is_sub_constraint(share, box));
  EXPECT_FALSE(is_sub_constraint(share, tag));
  EXPECT_FALSE(is_sub_constraint(share, read));
  EXPECT_TRUE(is_sub_constraint(share, send));
  EXPECT_TRUE(is_sub_constraint(share, share));
  EXPECT_TRUE(is_sub_constraint(share, alias));
  EXPECT_TRUE(is_sub_constraint(share, any));

  // #alias {ref, val, box, tag}
  EXPECT_FALSE(is_sub_constraint(alias, iso));
  EXPECT_FALSE(is_sub_constraint(alias, trn));
  EXPECT_FALSE(is_sub_constraint(alias, ref));
  EXPECT_FALSE(is_sub_constraint(alias, val));
  EXPECT_FALSE(is_sub_constraint(alias, box));
  EXPECT_FALSE(is_sub_constraint(alias, tag));
  EXPECT_FALSE(is_sub_constraint(alias, read));
  EXPECT_FALSE(is_sub_constraint(alias, send));
  EXPECT_FALSE(is_sub_constraint(alias, share));
  EXPECT_TRUE(is_sub_constraint(alias, alias));
  EXPECT_TRUE(is_sub_constraint(alias, any));

  // #any {iso, trn, ref, val, box, tag}
  EXPECT_FALSE(is_sub_constraint(any, iso));
  EXPECT_FALSE(is_sub_constraint(any, trn));
  EXPECT_FALSE(is_sub_constraint(any, ref));
  EXPECT_FALSE(is_sub_constraint(any, val));
  EXPECT_FALSE(is_sub_constraint(any, box));
  EXPECT_FALSE(is_sub_constraint(any, tag));
  EXPECT_FALSE(is_sub_constraint(any, read));
  EXPECT_FALSE(is_sub_constraint(any, send));
  EXPECT_FALSE(is_sub_constraint(any, share));
  EXPECT_FALSE(is_sub_constraint(any, alias));
  EXPECT_TRUE(is_sub_constraint(any, any));
}

TEST_F(CapTest, SubBoundConcrete)
{
  // If either cap is a specific cap:
  // Every possible instantiation of sub must be a subtype of every possible
  // instantiation of super.
  token_id caps[] = { iso, trn, ref, val, box, tag };
  int num_caps = sizeof(caps) / sizeof(token_id);
  for (int i = 0; i < num_caps; i++) {
    for (int j = 0; j < num_caps; j++) {
      ASSERT_EQ(is_sub_bound(caps[i], none, caps[j], none),
                   is_sub(caps[i], none, caps[j], none));
      ASSERT_EQ(is_sub_bound(caps[i], hat, caps[j], none),
                   is_sub(caps[i], hat, caps[j], none));
      ASSERT_EQ(is_sub_bound(caps[i], none, caps[j], hat),
                   is_sub(caps[i], none, caps[j], hat));
      ASSERT_EQ(is_sub_bound(caps[i], hat, caps[j], hat),
                   is_sub(caps[i], hat, caps[j], hat));
    }
  }
}

TEST_F(CapTest, SubBound)
{
  //
  // If both caps are generic caps,
  // we try to check aliasing and viewpoint adaptation
  // knowing that it comes from the original constraint
  // but the rules are fairly ad-hoc and subject to change

  // #read {ref, val, box}
  EXPECT_FALSE(is_sub_bound(read, none, iso, none));
  EXPECT_FALSE(is_sub_bound(read, none, trn, none));
  EXPECT_FALSE(is_sub_bound(read, none, ref, none));
  EXPECT_FALSE(is_sub_bound(read, none, val, none));
  EXPECT_TRUE(is_sub_bound(read, none, box, none));
  EXPECT_TRUE(is_sub_bound(read, none, tag, none));
  EXPECT_TRUE(is_sub_bound(read, none, read, none));
  EXPECT_FALSE(is_sub_bound(read, none, send, none));
  EXPECT_TRUE(is_sub_bound(read, none, share, none));
  EXPECT_TRUE(is_sub_bound(read, none, alias, none));
  EXPECT_FALSE(is_sub_bound(read, none, any, none));

  // #send {iso, val, tag}
  EXPECT_FALSE(is_sub_bound(send, none, iso, none));
  EXPECT_FALSE(is_sub_bound(send, none, trn, none));
  EXPECT_FALSE(is_sub_bound(send, none, ref, none));
  EXPECT_FALSE(is_sub_bound(send, none, val, none));
  EXPECT_FALSE(is_sub_bound(send, none, box, none));
  EXPECT_TRUE(is_sub_bound(send, none, tag, none));
  EXPECT_TRUE(is_sub_bound(send, none, read, none));
  EXPECT_TRUE(is_sub_bound(send, none, send, none));
  EXPECT_TRUE(is_sub_bound(send, none, share, none));
  EXPECT_TRUE(is_sub_bound(send, none, alias, none));
  EXPECT_TRUE(is_sub_bound(send, none, any, none));

  // #share {val, tag}
  EXPECT_FALSE(is_sub_bound(share, none, iso, none));
  EXPECT_FALSE(is_sub_bound(share, none, trn, none));
  EXPECT_FALSE(is_sub_bound(share, none, ref, none));
  EXPECT_FALSE(is_sub_bound(share, none, val, none));
  EXPECT_FALSE(is_sub_bound(share, none, box, none));
  EXPECT_TRUE(is_sub_bound(share, none, tag, none));
  EXPECT_FALSE(is_sub_bound(share, none, read, none));
  EXPECT_FALSE(is_sub_bound(share, none, send, none));
  EXPECT_TRUE(is_sub_bound(share, none, share, none));
  EXPECT_FALSE(is_sub_bound(share, none, alias, none));
  EXPECT_FALSE(is_sub_bound(share, none, any, none));

  // #alias {ref, val, box, tag}
  EXPECT_FALSE(is_sub_bound(alias, none, iso, none));
  EXPECT_FALSE(is_sub_bound(alias, none, trn, none));
  EXPECT_FALSE(is_sub_bound(alias, none, ref, none));
  EXPECT_FALSE(is_sub_bound(alias, none, val, none));
  EXPECT_FALSE(is_sub_bound(alias, none, box, none));
  EXPECT_TRUE(is_sub_bound(alias, none, tag, none));
  EXPECT_TRUE(is_sub_bound(alias, none, read, none));
  EXPECT_FALSE(is_sub_bound(alias, none, send, none));
  EXPECT_TRUE(is_sub_bound(alias, none, share, none));
  EXPECT_TRUE(is_sub_bound(alias, none, alias, none));
  EXPECT_FALSE(is_sub_bound(alias, none, any, none));

  // #any {iso, trn, ref, val, box, tag}
  EXPECT_FALSE(is_sub_bound(any, none, iso, none));
  EXPECT_FALSE(is_sub_bound(any, none, trn, none));
  EXPECT_FALSE(is_sub_bound(any, none, ref, none));
  EXPECT_FALSE(is_sub_bound(any, none, val, none));
  EXPECT_FALSE(is_sub_bound(any, none, box, none));
  EXPECT_TRUE(is_sub_bound(any, none, tag, none));
  EXPECT_TRUE(is_sub_bound(any, none, read, none));
  EXPECT_TRUE(is_sub_bound(any, none, send, none));
  EXPECT_TRUE(is_sub_bound(any, none, share, none));
  EXPECT_TRUE(is_sub_bound(any, none, alias, none));
  EXPECT_TRUE(is_sub_bound(any, none, any, none));
}

TEST_F(CapTest, Compat)
{
  // Every possible instantiation of left must be compatible with every possible
  // instantiation of right.

  // iso
  EXPECT_TRUE(is_compat(iso, iso));
  EXPECT_FALSE(is_compat(iso, trn));
  EXPECT_FALSE(is_compat(iso, ref));
  EXPECT_FALSE(is_compat(iso, val));
  EXPECT_FALSE(is_compat(iso, box));
  EXPECT_TRUE(is_compat(iso, tag));
  EXPECT_FALSE(is_compat(iso, read));
  EXPECT_FALSE(is_compat(iso, send));
  EXPECT_FALSE(is_compat(iso, share));
  EXPECT_FALSE(is_compat(iso, alias));
  EXPECT_FALSE(is_compat(iso, any));

  // trn
  EXPECT_FALSE(is_compat(trn, iso));
  EXPECT_TRUE(is_compat(trn, trn));
  EXPECT_FALSE(is_compat(trn, ref));
  EXPECT_FALSE(is_compat(trn, val));
  EXPECT_TRUE(is_compat(trn, box));
  EXPECT_TRUE(is_compat(trn, tag));
  EXPECT_FALSE(is_compat(trn, read));
  EXPECT_FALSE(is_compat(trn, send));
  EXPECT_FALSE(is_compat(trn, share));
  EXPECT_FALSE(is_compat(trn, alias));
  EXPECT_FALSE(is_compat(trn, any));

  // ref
  EXPECT_FALSE(is_compat(ref, iso));
  EXPECT_FALSE(is_compat(ref, trn));
  EXPECT_TRUE(is_compat(ref, ref));
  EXPECT_FALSE(is_compat(ref, val));
  EXPECT_TRUE(is_compat(ref, box));
  EXPECT_TRUE(is_compat(ref, tag));
  EXPECT_FALSE(is_compat(ref, read));
  EXPECT_FALSE(is_compat(ref, send));
  EXPECT_FALSE(is_compat(ref, share));
  EXPECT_FALSE(is_compat(ref, alias));
  EXPECT_FALSE(is_compat(ref, any));

  // val
  EXPECT_FALSE(is_compat(val, iso));
  EXPECT_FALSE(is_compat(val, trn));
  EXPECT_FALSE(is_compat(val, ref));
  EXPECT_TRUE(is_compat(val, val));
  EXPECT_TRUE(is_compat(val, box));
  EXPECT_TRUE(is_compat(val, tag));
  EXPECT_FALSE(is_compat(val, read));
  EXPECT_FALSE(is_compat(val, send));
  EXPECT_TRUE(is_compat(val, share));
  EXPECT_FALSE(is_compat(val, alias));
  EXPECT_FALSE(is_compat(val, any));

  // box
  EXPECT_FALSE(is_compat(box, iso));
  EXPECT_TRUE(is_compat(box, trn));
  EXPECT_TRUE(is_compat(box, ref));
  EXPECT_TRUE(is_compat(box, val));
  EXPECT_TRUE(is_compat(box, box));
  EXPECT_TRUE(is_compat(box, tag));
  EXPECT_TRUE(is_compat(box, read));
  EXPECT_FALSE(is_compat(box, send));
  EXPECT_TRUE(is_compat(box, share));
  EXPECT_TRUE(is_compat(box, alias));
  EXPECT_FALSE(is_compat(box, any));

  // tag
  EXPECT_TRUE(is_compat(tag, iso));
  EXPECT_TRUE(is_compat(tag, trn));
  EXPECT_TRUE(is_compat(tag, ref));
  EXPECT_TRUE(is_compat(tag, val));
  EXPECT_TRUE(is_compat(tag, box));
  EXPECT_TRUE(is_compat(tag, tag));
  EXPECT_TRUE(is_compat(tag, read));
  EXPECT_TRUE(is_compat(tag, send));
  EXPECT_TRUE(is_compat(tag, share));
  EXPECT_TRUE(is_compat(tag, alias));
  EXPECT_TRUE(is_compat(tag, any));

  // #read {ref, val, box}
  EXPECT_FALSE(is_compat(read, iso));
  EXPECT_FALSE(is_compat(read, trn));
  EXPECT_FALSE(is_compat(read, ref));
  EXPECT_FALSE(is_compat(read, val));
  EXPECT_TRUE(is_compat(read, box));
  EXPECT_TRUE(is_compat(read, tag));
  EXPECT_FALSE(is_compat(read, read));
  EXPECT_FALSE(is_compat(read, send));
  EXPECT_FALSE(is_compat(read, share));
  EXPECT_FALSE(is_compat(read, alias));
  EXPECT_FALSE(is_compat(read, any));

  // #send {iso, val, tag}
  EXPECT_FALSE(is_compat(send, iso));
  EXPECT_FALSE(is_compat(send, trn));
  EXPECT_FALSE(is_compat(send, ref));
  EXPECT_FALSE(is_compat(send, val));
  EXPECT_FALSE(is_compat(send, box));
  EXPECT_TRUE(is_compat(send, tag));
  EXPECT_FALSE(is_compat(send, read));
  EXPECT_FALSE(is_compat(send, send));
  EXPECT_FALSE(is_compat(send, share));
  EXPECT_FALSE(is_compat(send, alias));
  EXPECT_FALSE(is_compat(send, any));

  // #share {val, tag}
  EXPECT_FALSE(is_compat(share, iso));
  EXPECT_FALSE(is_compat(share, trn));
  EXPECT_FALSE(is_compat(share, ref));
  EXPECT_TRUE(is_compat(share, val));
  EXPECT_TRUE(is_compat(share, box));
  EXPECT_TRUE(is_compat(share, tag));
  EXPECT_FALSE(is_compat(share, read));
  EXPECT_FALSE(is_compat(share, send));
  EXPECT_TRUE(is_compat(share, share));
  EXPECT_FALSE(is_compat(share, alias));
  EXPECT_FALSE(is_compat(share, any));

  // #alias {ref, val, box, tag}
  EXPECT_FALSE(is_compat(alias, iso));
  EXPECT_FALSE(is_compat(alias, trn));
  EXPECT_FALSE(is_compat(alias, ref));
  EXPECT_FALSE(is_compat(alias, val));
  EXPECT_TRUE(is_compat(alias, box));
  EXPECT_TRUE(is_compat(alias, tag));
  EXPECT_FALSE(is_compat(alias, read));
  EXPECT_FALSE(is_compat(alias, send));
  EXPECT_FALSE(is_compat(alias, share));
  EXPECT_FALSE(is_compat(alias, alias));
  EXPECT_FALSE(is_compat(alias, any));

  // #any {iso, trn, ref, val, box, tag}
  EXPECT_FALSE(is_compat(any, iso));
  EXPECT_FALSE(is_compat(any, trn));
  EXPECT_FALSE(is_compat(any, ref));
  EXPECT_FALSE(is_compat(any, val));
  EXPECT_FALSE(is_compat(any, box));
  EXPECT_TRUE(is_compat(any, tag));
  EXPECT_FALSE(is_compat(any, read));
  EXPECT_FALSE(is_compat(any, send));
  EXPECT_FALSE(is_compat(any, share));
  EXPECT_FALSE(is_compat(any, alias));
  EXPECT_FALSE(is_compat(any, any));
}
TEST_F(CapTest, ViewpointLower)
{
  // iso->
  ASSERT_EQ(lower_viewpoint(iso, iso), iso);
  ASSERT_EQ(lower_viewpoint(iso, trn), tag);
  ASSERT_EQ(lower_viewpoint(iso, ref), tag);
  ASSERT_EQ(lower_viewpoint(iso, val), val);
  ASSERT_EQ(lower_viewpoint(iso, box), tag);
  ASSERT_EQ(lower_viewpoint(iso, tag), tag);

  // trn->
  ASSERT_EQ(lower_viewpoint(trn, iso), iso);
  ASSERT_EQ(lower_viewpoint(trn, trn), box);
  ASSERT_EQ(lower_viewpoint(trn, ref), box);
  ASSERT_EQ(lower_viewpoint(trn, val), val);
  ASSERT_EQ(lower_viewpoint(trn, box), box);
  ASSERT_EQ(lower_viewpoint(trn, tag), tag);

  // ref->
  ASSERT_EQ(lower_viewpoint(ref, iso), iso);
  ASSERT_EQ(lower_viewpoint(ref, trn), trn);
  ASSERT_EQ(lower_viewpoint(ref, ref), ref);
  ASSERT_EQ(lower_viewpoint(ref, val), val);
  ASSERT_EQ(lower_viewpoint(ref, box), box);
  ASSERT_EQ(lower_viewpoint(ref, tag), tag);

  // val->
  ASSERT_EQ(lower_viewpoint(val, iso), val);
  ASSERT_EQ(lower_viewpoint(val, trn), val);
  ASSERT_EQ(lower_viewpoint(val, ref), val);
  ASSERT_EQ(lower_viewpoint(val, val), val);
  ASSERT_EQ(lower_viewpoint(val, box), val);
  ASSERT_EQ(lower_viewpoint(val, tag), tag);

  // box->
  ASSERT_EQ(lower_viewpoint(box, iso), tag);
  ASSERT_EQ(lower_viewpoint(box, trn), box);
  ASSERT_EQ(lower_viewpoint(box, ref), box);
  ASSERT_EQ(lower_viewpoint(box, val), val);
  ASSERT_EQ(lower_viewpoint(box, box), box);
  ASSERT_EQ(lower_viewpoint(box, tag), tag);
}
TEST_F(CapTest, ViewpointUpper)
{
  // iso->
  ASSERT_EQ(upper_viewpoint(iso, iso), iso);
  ASSERT_EQ(upper_viewpoint(iso, trn), tag);
  ASSERT_EQ(upper_viewpoint(iso, ref), tag);
  ASSERT_EQ(upper_viewpoint(iso, val), val);
  ASSERT_EQ(upper_viewpoint(iso, box), tag);
  ASSERT_EQ(upper_viewpoint(iso, tag), tag);

  // trn->
  ASSERT_EQ(upper_viewpoint(trn, iso), iso);
  ASSERT_EQ(upper_viewpoint(trn, trn), box);
  ASSERT_EQ(upper_viewpoint(trn, ref), box);
  ASSERT_EQ(upper_viewpoint(trn, val), val);
  ASSERT_EQ(upper_viewpoint(trn, box), box);
  ASSERT_EQ(upper_viewpoint(trn, tag), tag);

  // ref->
  ASSERT_EQ(upper_viewpoint(ref, iso), iso);
  ASSERT_EQ(upper_viewpoint(ref, trn), trn);
  ASSERT_EQ(upper_viewpoint(ref, ref), ref);
  ASSERT_EQ(upper_viewpoint(ref, val), val);
  ASSERT_EQ(upper_viewpoint(ref, box), box);
  ASSERT_EQ(upper_viewpoint(ref, tag), tag);

  // val->
  ASSERT_EQ(upper_viewpoint(val, iso), val);
  ASSERT_EQ(upper_viewpoint(val, trn), val);
  ASSERT_EQ(upper_viewpoint(val, ref), val);
  ASSERT_EQ(upper_viewpoint(val, val), val);
  ASSERT_EQ(upper_viewpoint(val, box), val);
  ASSERT_EQ(upper_viewpoint(val, tag), tag);

  // box->
  ASSERT_EQ(upper_viewpoint(box, iso), tag);
  ASSERT_EQ(upper_viewpoint(box, trn), box);
  ASSERT_EQ(upper_viewpoint(box, ref), box);
  ASSERT_EQ(upper_viewpoint(box, val), val);
  ASSERT_EQ(upper_viewpoint(box, box), box);
  ASSERT_EQ(upper_viewpoint(box, tag), tag);
}
