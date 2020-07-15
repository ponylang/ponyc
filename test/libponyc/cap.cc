#include <gtest/gtest.h>
#include <platform.h>

#include <type/cap.h>

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

  bool is_sub(token_id sub, token_id super)
  {
    return is_cap_sub_cap(sub, none, super, none);
  }

  bool is_sub_constraint(token_id sub, token_id super)
  {
    return is_cap_sub_cap_constraint(sub, none, super, none);
  }

  bool is_sub_bound(token_id sub, token_id super)
  {
    return is_cap_sub_cap_bound(sub, none, super, none);
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

TEST_F(CapTest, Sub)
{
  // Every possible instantiation of sub must be a subtype of every possible
  // instantiation of super.

  // iso
  EXPECT_TRUE(is_sub(iso, iso));
  EXPECT_TRUE(is_sub(iso, trn));
  EXPECT_TRUE(is_sub(iso, ref));
  EXPECT_TRUE(is_sub(iso, val));
  EXPECT_TRUE(is_sub(iso, box));
  EXPECT_TRUE(is_sub(iso, tag));
  EXPECT_TRUE(is_sub(iso, read));
  EXPECT_TRUE(is_sub(iso, send));
  EXPECT_TRUE(is_sub(iso, share));
  EXPECT_TRUE(is_sub(iso, alias));
  EXPECT_TRUE(is_sub(iso, any));

  // trn
  EXPECT_FALSE(is_sub(trn, iso));
  EXPECT_TRUE(is_sub(trn, trn));
  EXPECT_TRUE(is_sub(trn, ref));
  EXPECT_TRUE(is_sub(trn, val));
  EXPECT_TRUE(is_sub(trn, box));
  EXPECT_TRUE(is_sub(trn, tag));
  EXPECT_TRUE(is_sub(trn, read));
  EXPECT_FALSE(is_sub(trn, send));
  EXPECT_TRUE(is_sub(trn, share));
  EXPECT_TRUE(is_sub(trn, alias));
  EXPECT_FALSE(is_sub(trn, any));

  // ref
  EXPECT_FALSE(is_sub(ref, iso));
  EXPECT_FALSE(is_sub(ref, trn));
  EXPECT_TRUE(is_sub(ref, ref));
  EXPECT_FALSE(is_sub(ref, val));
  EXPECT_TRUE(is_sub(ref, box));
  EXPECT_TRUE(is_sub(ref, tag));
  EXPECT_FALSE(is_sub(ref, read));
  EXPECT_FALSE(is_sub(ref, send));
  EXPECT_FALSE(is_sub(ref, share));
  EXPECT_FALSE(is_sub(ref, alias));
  EXPECT_FALSE(is_sub(ref, any));

  // val
  EXPECT_FALSE(is_sub(val, iso));
  EXPECT_FALSE(is_sub(val, trn));
  EXPECT_FALSE(is_sub(val, ref));
  EXPECT_TRUE(is_sub(val, val));
  EXPECT_TRUE(is_sub(val, box));
  EXPECT_TRUE(is_sub(val, tag));
  EXPECT_FALSE(is_sub(val, read));
  EXPECT_FALSE(is_sub(val, send));
  EXPECT_TRUE(is_sub(val, share));
  EXPECT_FALSE(is_sub(val, alias));
  EXPECT_FALSE(is_sub(val, any));

  // box
  EXPECT_FALSE(is_sub(box, iso));
  EXPECT_FALSE(is_sub(box, trn));
  EXPECT_FALSE(is_sub(box, ref));
  EXPECT_FALSE(is_sub(box, val));
  EXPECT_TRUE(is_sub(box, box));
  EXPECT_TRUE(is_sub(box, tag));
  EXPECT_FALSE(is_sub(box, read));
  EXPECT_FALSE(is_sub(box, send));
  EXPECT_FALSE(is_sub(box, share));
  EXPECT_FALSE(is_sub(box, alias));
  EXPECT_FALSE(is_sub(box, any));

  // tag
  EXPECT_FALSE(is_sub(tag, iso));
  EXPECT_FALSE(is_sub(tag, trn));
  EXPECT_FALSE(is_sub(tag, ref));
  EXPECT_FALSE(is_sub(tag, val));
  EXPECT_FALSE(is_sub(tag, box));
  EXPECT_TRUE(is_sub(tag, tag));
  EXPECT_FALSE(is_sub(tag, read));
  EXPECT_FALSE(is_sub(tag, send));
  EXPECT_FALSE(is_sub(tag, share));
  EXPECT_FALSE(is_sub(tag, alias));
  EXPECT_FALSE(is_sub(tag, any));

  // #read {ref, val, box}
  EXPECT_FALSE(is_sub(read, iso));
  EXPECT_FALSE(is_sub(read, trn));
  EXPECT_FALSE(is_sub(read, ref));
  EXPECT_FALSE(is_sub(read, val));
  EXPECT_TRUE(is_sub(read, box));
  EXPECT_TRUE(is_sub(read, tag));
  EXPECT_FALSE(is_sub(read, read));
  EXPECT_FALSE(is_sub(read, send));
  EXPECT_FALSE(is_sub(read, share));
  EXPECT_FALSE(is_sub(read, alias));
  EXPECT_FALSE(is_sub(read, any));

  // #send {iso, val, tag}
  EXPECT_FALSE(is_sub(send, iso));
  EXPECT_FALSE(is_sub(send, trn));
  EXPECT_FALSE(is_sub(send, ref));
  EXPECT_FALSE(is_sub(send, val));
  EXPECT_FALSE(is_sub(send, box));
  EXPECT_TRUE(is_sub(send, tag));
  EXPECT_FALSE(is_sub(send, read));
  EXPECT_FALSE(is_sub(send, send));
  EXPECT_FALSE(is_sub(send, share));
  EXPECT_FALSE(is_sub(send, alias));
  EXPECT_FALSE(is_sub(send, any));

  // #share {val, tag}
  EXPECT_FALSE(is_sub(share, iso));
  EXPECT_FALSE(is_sub(share, trn));
  EXPECT_FALSE(is_sub(share, ref));
  EXPECT_FALSE(is_sub(share, val));
  EXPECT_FALSE(is_sub(share, box));
  EXPECT_TRUE(is_sub(share, tag));
  EXPECT_FALSE(is_sub(share, read));
  EXPECT_FALSE(is_sub(share, send));
  EXPECT_FALSE(is_sub(share, share));
  EXPECT_FALSE(is_sub(share, alias));
  EXPECT_FALSE(is_sub(share, any));

  // #alias {ref, val, box, tag}
  EXPECT_FALSE(is_sub(alias, iso));
  EXPECT_FALSE(is_sub(alias, trn));
  EXPECT_FALSE(is_sub(alias, ref));
  EXPECT_FALSE(is_sub(alias, val));
  EXPECT_FALSE(is_sub(alias, box));
  EXPECT_TRUE(is_sub(alias, tag));
  EXPECT_FALSE(is_sub(alias, read));
  EXPECT_FALSE(is_sub(alias, send));
  EXPECT_FALSE(is_sub(alias, share));
  EXPECT_FALSE(is_sub(alias, alias));
  EXPECT_FALSE(is_sub(alias, any));

  // #any {iso, trn, ref, val, box, tag}
  EXPECT_FALSE(is_sub(any, iso));
  EXPECT_FALSE(is_sub(any, trn));
  EXPECT_FALSE(is_sub(any, ref));
  EXPECT_FALSE(is_sub(any, val));
  EXPECT_FALSE(is_sub(any, box));
  EXPECT_TRUE(is_sub(any, tag));
  EXPECT_FALSE(is_sub(any, read));
  EXPECT_FALSE(is_sub(any, send));
  EXPECT_FALSE(is_sub(any, share));
  EXPECT_FALSE(is_sub(any, alias));
  EXPECT_FALSE(is_sub(any, any));
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

TEST_F(CapTest, SubBound)
{
  // If either cap is a specific cap:
  // Every possible instantiation of sub must be a subtype of every possible
  // instantiation of super.
  //
  // If both caps are generic caps:
  // Every possible instantiation of the super rcap must be a supertype of some
  // instantiation of the sub rcap (but not necessarily the same instantiation
  // of the sub rcap).

  // iso
  EXPECT_TRUE(is_sub_bound(iso, iso));
  EXPECT_TRUE(is_sub_bound(iso, trn));
  EXPECT_TRUE(is_sub_bound(iso, ref));
  EXPECT_TRUE(is_sub_bound(iso, val));
  EXPECT_TRUE(is_sub_bound(iso, box));
  EXPECT_TRUE(is_sub_bound(iso, tag));
  EXPECT_TRUE(is_sub_bound(iso, read));
  EXPECT_TRUE(is_sub_bound(iso, send));
  EXPECT_TRUE(is_sub_bound(iso, share));
  EXPECT_TRUE(is_sub_bound(iso, alias));
  EXPECT_TRUE(is_sub_bound(iso, any));

  // trn
  EXPECT_FALSE(is_sub_bound(trn, iso));
  EXPECT_TRUE(is_sub_bound(trn, trn));
  EXPECT_TRUE(is_sub_bound(trn, ref));
  EXPECT_TRUE(is_sub_bound(trn, val));
  EXPECT_TRUE(is_sub_bound(trn, box));
  EXPECT_TRUE(is_sub_bound(trn, tag));
  EXPECT_TRUE(is_sub_bound(trn, read));
  EXPECT_FALSE(is_sub_bound(trn, send));
  EXPECT_TRUE(is_sub_bound(trn, share));
  EXPECT_TRUE(is_sub_bound(trn, alias));
  EXPECT_FALSE(is_sub_bound(trn, any));

  // ref
  EXPECT_FALSE(is_sub_bound(ref, iso));
  EXPECT_FALSE(is_sub_bound(ref, trn));
  EXPECT_TRUE(is_sub_bound(ref, ref));
  EXPECT_FALSE(is_sub_bound(ref, val));
  EXPECT_TRUE(is_sub_bound(ref, box));
  EXPECT_TRUE(is_sub_bound(ref, tag));
  EXPECT_FALSE(is_sub_bound(ref, read));
  EXPECT_FALSE(is_sub_bound(ref, send));
  EXPECT_FALSE(is_sub_bound(ref, share));
  EXPECT_FALSE(is_sub_bound(ref, alias));
  EXPECT_FALSE(is_sub_bound(ref, any));

  // val
  EXPECT_FALSE(is_sub_bound(val, iso));
  EXPECT_FALSE(is_sub_bound(val, trn));
  EXPECT_FALSE(is_sub_bound(val, ref));
  EXPECT_TRUE(is_sub_bound(val, val));
  EXPECT_TRUE(is_sub_bound(val, box));
  EXPECT_TRUE(is_sub_bound(val, tag));
  EXPECT_FALSE(is_sub_bound(val, read));
  EXPECT_FALSE(is_sub_bound(val, send));
  EXPECT_TRUE(is_sub_bound(val, share));
  EXPECT_FALSE(is_sub_bound(val, alias));
  EXPECT_FALSE(is_sub_bound(val, any));

  // box
  EXPECT_FALSE(is_sub_bound(box, iso));
  EXPECT_FALSE(is_sub_bound(box, trn));
  EXPECT_FALSE(is_sub_bound(box, ref));
  EXPECT_FALSE(is_sub_bound(box, val));
  EXPECT_TRUE(is_sub_bound(box, box));
  EXPECT_TRUE(is_sub_bound(box, tag));
  EXPECT_FALSE(is_sub_bound(box, read));
  EXPECT_FALSE(is_sub_bound(box, send));
  EXPECT_FALSE(is_sub_bound(box, share));
  EXPECT_FALSE(is_sub_bound(box, alias));
  EXPECT_FALSE(is_sub_bound(box, any));

  // tag
  EXPECT_FALSE(is_sub_bound(tag, iso));
  EXPECT_FALSE(is_sub_bound(tag, trn));
  EXPECT_FALSE(is_sub_bound(tag, ref));
  EXPECT_FALSE(is_sub_bound(tag, val));
  EXPECT_FALSE(is_sub_bound(tag, box));
  EXPECT_TRUE(is_sub_bound(tag, tag));
  EXPECT_FALSE(is_sub_bound(tag, read));
  EXPECT_FALSE(is_sub_bound(tag, send));
  EXPECT_FALSE(is_sub_bound(tag, share));
  EXPECT_FALSE(is_sub_bound(tag, alias));
  EXPECT_FALSE(is_sub_bound(tag, any));

  // #read {ref, val, box}
  EXPECT_FALSE(is_sub_bound(read, iso));
  EXPECT_FALSE(is_sub_bound(read, trn));
  EXPECT_FALSE(is_sub_bound(read, ref));
  EXPECT_FALSE(is_sub_bound(read, val));
  EXPECT_TRUE(is_sub_bound(read, box));
  EXPECT_TRUE(is_sub_bound(read, tag));
  EXPECT_TRUE(is_sub_bound(read, read));
  EXPECT_FALSE(is_sub_bound(read, send));
  EXPECT_TRUE(is_sub_bound(read, share));
  EXPECT_TRUE(is_sub_bound(read, alias));
  EXPECT_FALSE(is_sub_bound(read, any));

  // #send {iso, val, tag}
  EXPECT_FALSE(is_sub_bound(send, iso));
  EXPECT_FALSE(is_sub_bound(send, trn));
  EXPECT_FALSE(is_sub_bound(send, ref));
  EXPECT_FALSE(is_sub_bound(send, val));
  EXPECT_FALSE(is_sub_bound(send, box));
  EXPECT_TRUE(is_sub_bound(send, tag));
  EXPECT_TRUE(is_sub_bound(send, read));
  EXPECT_TRUE(is_sub_bound(send, send));
  EXPECT_TRUE(is_sub_bound(send, share));
  EXPECT_TRUE(is_sub_bound(send, alias));
  EXPECT_TRUE(is_sub_bound(send, any));

  // #share {val, tag}
  EXPECT_FALSE(is_sub_bound(share, iso));
  EXPECT_FALSE(is_sub_bound(share, trn));
  EXPECT_FALSE(is_sub_bound(share, ref));
  EXPECT_FALSE(is_sub_bound(share, val));
  EXPECT_FALSE(is_sub_bound(share, box));
  EXPECT_TRUE(is_sub_bound(share, tag));
  EXPECT_FALSE(is_sub_bound(share, read));
  EXPECT_FALSE(is_sub_bound(share, send));
  EXPECT_TRUE(is_sub_bound(share, share));
  EXPECT_FALSE(is_sub_bound(share, alias));
  EXPECT_FALSE(is_sub_bound(share, any));

  // #alias {ref, val, box, tag}
  EXPECT_FALSE(is_sub_bound(alias, iso));
  EXPECT_FALSE(is_sub_bound(alias, trn));
  EXPECT_FALSE(is_sub_bound(alias, ref));
  EXPECT_FALSE(is_sub_bound(alias, val));
  EXPECT_FALSE(is_sub_bound(alias, box));
  EXPECT_TRUE(is_sub_bound(alias, tag));
  EXPECT_TRUE(is_sub_bound(alias, read));
  EXPECT_FALSE(is_sub_bound(alias, send));
  EXPECT_TRUE(is_sub_bound(alias, share));
  EXPECT_TRUE(is_sub_bound(alias, alias));
  EXPECT_FALSE(is_sub_bound(alias, any));

  // #any {iso, trn, ref, val, box, tag}
  EXPECT_FALSE(is_sub_bound(any, iso));
  EXPECT_FALSE(is_sub_bound(any, trn));
  EXPECT_FALSE(is_sub_bound(any, ref));
  EXPECT_FALSE(is_sub_bound(any, val));
  EXPECT_FALSE(is_sub_bound(any, box));
  EXPECT_TRUE(is_sub_bound(any, tag));
  EXPECT_TRUE(is_sub_bound(any, read));
  EXPECT_TRUE(is_sub_bound(any, send));
  EXPECT_TRUE(is_sub_bound(any, share));
  EXPECT_TRUE(is_sub_bound(any, alias));
  EXPECT_TRUE(is_sub_bound(any, any));
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
