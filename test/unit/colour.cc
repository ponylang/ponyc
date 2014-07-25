extern "C" {
#include "../../src/libponyc/ast/builder.h"
#include "../../src/libponyc/codegen/colour.h"
#include "../../src/libponyc/ds/stringtab.h"
}
#include "util.h"
#include <gtest/gtest.h>


class ColourTest: public testing::Test
{};


TEST(ColourTest, SingleTypeSingleFunction)
{
  const char* def =
    "(module"
    "  (class (id Foo) x x x"
    "    (members (fun ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, FunctionNotFound)
{
  const char* def =
    "(module"
    "  (class (id Foo) x x x"
    "    (members (fun ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(-1, painter_get_colour(p, stringtab("bar")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, MultiTypeMultiFunction)
{
  const char* def =
    "(module"
    "  (class (id Foo) x x x"
    "    (members"
    "      (fun ref (id foo) x x x x x)"
    "      (fun ref (id bar) x x x x x)))"
    "  (class (id Bar) x x x"
    "    (members"
    "      (fun ref (id bar) x x x x x)"
    "      (fun ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_colour(p, stringtab("bar")));

  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 0)));
  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 1)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, ThreeTypeTwoFunctionForcedSparse)
{
  const char* def =
    "(module"
    "  (class (id A) x x x"
    "    (members"
    "      (fun ref (id d) x x x x x)"
    "      (fun ref (id e) x x x x x)))"
    "  (class (id B) x x x"
    "    (members"
    "      (fun ref (id e) x x x x x)"
    "      (fun ref (id f) x x x x x)))"
    "  (class (id C) x x x"
    "    (members"
    "      (fun ref (id d) x x x x x)"
    "      (fun ref (id f) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("d")));
  ASSERT_EQ(1, painter_get_colour(p, stringtab("e")));
  ASSERT_EQ(2, painter_get_colour(p, stringtab("f")));

  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 0)));
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 1)));
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 2)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, FieldsSkipped)
{
  const char* def =
    "(module"
    "  (class (id Foo) x x x"
    "    (members"
    "      (fvar (id wombat) x x)"
    "      (flet (id aardvark) x x)"
    "      (fun ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, ConstructorsSkipped)
{
  const char* def =
    "(module"
    "  (class (id Foo) x x x"
    "    (members"
    "      (new ref (id wombat) x x x x x)"
    "      (fun ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, Behaviour)
{
  const char* def =
    "(module"
    "  (class (id Foo) x x x"
    "    (members"
    "      (be ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, Actor)
{
  const char* def =
    "(module"
    "  (actor (id Foo) x x x"
    "    (members"
    "      (be ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, Data)
{
  const char* def =
    "(module"
    "  (data (id Foo) x x x"
    "    (members"
    "      (be ref (id foo) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, TraitsIgnored)
{
  const char* def =
    "(module"
    "  (class (id Foo) x x x"
    "    (members"
    "      (fun ref (id foo) x x x x x)))"
    "  (trait (id Bar) x x x"
    "    (members"
    "      (fun x (id bar) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(-1, painter_get_colour(p, stringtab("bar")));
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_child(module)));

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, With64Types)
{
  const char* def =
    "(module"
    "  (class (id T1) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T2) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T3) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T4) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T5) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T6) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T7) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T8) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T9) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T10) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T11) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T12) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T13) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T14) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T15) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T16) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T17) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T18) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T19) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T20) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T21) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T22) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T23) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T24) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T25) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T26) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T27) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T28) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T29) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T30) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T31) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T32) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T33) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T34) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T35) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T36) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T37) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T38) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T39) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T40) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T41) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T42) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T43) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T44) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T45) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T46) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T47) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T48) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T49) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T50) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T51) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T52) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T53) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T54) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T55) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T56) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T57) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T58) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T59) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T60) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T61) x x x (members (fun ref (id unique) x x x x x)))"
    "  (class (id T62) x x x"
    "    (members"
    "      (fun ref (id wibble) x x x x x)"
    "      (fun ref (id wobble) x x x x x)))"
    "  (class (id T63) x x x"
    "    (members"
    "      (fun ref (id foo) x x x x x)"
    "      (fun ref (id bar) x x x x x)"
    "      (fun ref (id wombat) x x x x x)))"
    "  (class (id T64) x x x"
    "    (members"
    "      (fun ref (id foo) x x x x x)"
    "      (fun ref (id bar) x x x x x)"
    "      (fun ref (id aardvark) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(0, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(1, painter_get_colour(p, stringtab("bar")));
  ASSERT_EQ(2, painter_get_colour(p, stringtab("wombat")));
  ASSERT_EQ(2, painter_get_colour(p, stringtab("aardvark")));
  ASSERT_EQ(0, painter_get_colour(p, stringtab("wibble")));
  ASSERT_EQ(1, painter_get_colour(p, stringtab("wobble")));
  ASSERT_EQ(0, painter_get_colour(p, stringtab("unique")));

  ASSERT_EQ(1, painter_get_vtable_size(p, ast_childidx(module, 0)));   // T1
  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 10)));  // T11
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 20)));  // T21
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 30)));  // T31
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_childidx(module, 40)));  // T41
  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 50)));  // T51
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_childidx(module, 60)));  // T61
  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 61)));  // T62
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 62)));  // T63
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 63)));  // T64

  painter_free(p);
  ast_free(module);
}


TEST(ColourTest, With66Types)
{
  const char* def =
    "(module"
    "  (class (id T1) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T2) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T3) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T4) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T5) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T6) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T7) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T8) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T9) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T10) x x x (members (fun ref (id foo) x x x x x)))"
    "  (class (id T11) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T12) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T13) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T14) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T15) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T16) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T17) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T18) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T19) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T20) x x x (members (fun ref (id bar) x x x x x)))"
    "  (class (id T21) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T22) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T23) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T24) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T25) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T26) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T27) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T28) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T29) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T30) x x x (members (fun ref (id wombat) x x x x x)))"
    "  (class (id T31) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T32) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T33) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T34) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T35) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T36) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T37) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T38) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T39) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T40) x x x (members (fun ref (id aardvark) x x x x x)))"
    "  (class (id T41) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T42) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T43) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T44) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T45) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T46) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T47) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T48) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T49) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T50) x x x (members (fun ref (id wibble) x x x x x)))"
    "  (class (id T51) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T52) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T53) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T54) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T55) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T56) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T57) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T58) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T59) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T60) x x x (members (fun ref (id wobble) x x x x x)))"
    "  (class (id T61) x x x (members (fun ref (id unique) x x x x x)))"
    "  (class (id T62) x x x"
    "    (members"
    "      (fun ref (id wibble) x x x x x)"
    "      (fun ref (id wobble) x x x x x)))"
    "  (class (id T63) x x x"
    "    (members"
    "      (fun ref (id foo) x x x x x)"
    "      (fun ref (id bar) x x x x x)"
    "      (fun ref (id wombat) x x x x x)))"
    "  (class (id T64) x x x"
    "    (members"
    "      (fun ref (id foo) x x x x x)"
    "      (fun ref (id bar) x x x x x)"
    "      (fun ref (id aardvark) x x x x x)))"
    "  (class (id T65) x x x"
    "    (members"
    "      (fun ref (id bar) x x x x x)"
    "      (fun ref (id wibble) x x x x x)))"
    "  (class (id T66) x x x"
    "    (members"
    "      (fun ref (id foo) x x x x x)"
    "      (fun ref (id wombat) x x x x x)"
    "      (fun ref (id wibble) x x x x x))))";

  ast_t* module = parse_test_module(def);
  ASSERT_NE((void*)NULL, module);

  painter_t* p = painter_create();
  painter_colour(p, module);

  ASSERT_EQ(1, painter_get_colour(p, stringtab("foo")));
  ASSERT_EQ(2, painter_get_colour(p, stringtab("bar")));
  ASSERT_EQ(3, painter_get_colour(p, stringtab("wombat")));
  ASSERT_EQ(0, painter_get_colour(p, stringtab("aardvark")));
  ASSERT_EQ(0, painter_get_colour(p, stringtab("wibble")));
  ASSERT_EQ(1, painter_get_colour(p, stringtab("wobble")));
  ASSERT_EQ(0, painter_get_colour(p, stringtab("unique")));

  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 1)));   // T2
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 10)));  // T11
  ASSERT_EQ(4, painter_get_vtable_size(p, ast_childidx(module, 20)));  // T21
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_childidx(module, 30)));  // T31
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_childidx(module, 40)));  // T41
  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 50)));  // T51
  ASSERT_EQ(1, painter_get_vtable_size(p, ast_childidx(module, 60)));  // T61
  ASSERT_EQ(2, painter_get_vtable_size(p, ast_childidx(module, 61)));  // T62
  ASSERT_EQ(4, painter_get_vtable_size(p, ast_childidx(module, 62)));  // T63
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 63)));  // T64
  ASSERT_EQ(3, painter_get_vtable_size(p, ast_childidx(module, 64)));  // T65
  ASSERT_EQ(4, painter_get_vtable_size(p, ast_childidx(module, 65)));  // T66

  painter_free(p);
  ast_free(module);
}
