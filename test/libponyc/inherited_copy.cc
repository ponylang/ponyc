#include <gtest/gtest.h>
#include <platform.h>

#include <ast/error.h>

#include <string.h>

#include <vector>

#include "util.h"

// Code a type inherits is checked for that type, and each error found there
// carries the note "in method 'm' inherited by C from T". The same error in a
// type's own code, or in the trait's own check, carries no such note.
class InheritedCopyTest : public PassTest
{
protected:
  virtual void SetUp()
  {
    PassTest::SetUp();
    set_builtin(NULL);
  }

  // For each error reported, the copy note it carries, or "" for none.
  std::vector<std::string> copy_notes()
  {
    std::vector<std::string> notes;

    for(errormsg_t* e = errors_get_first(opt.check.errors); e != NULL;
      e = e->next)
    {
      std::string note;

      for(errormsg_t* f = e->frame; f != NULL; f = f->frame)
      {
        if((strncmp(f->msg, COPY_NOTE_START, strlen(COPY_NOTE_START)) == 0) &&
          (strstr(f->msg, COPY_NOTE_INHERITED) != NULL))
          note = f->msg;
      }

      notes.push_back(note);
    }

    return notes;
  }
};

TEST_F(InheritedCopyTest, NamedCopy)
{
  // The trait's check reports f's body, then C's copy of f reports it again
  // with the note.
  const char* src =
    "trait T\n"
    "  fun f(): U8 => \"x\"\n"

    "class C is T\n";

  const char* errs[] = {
    "function body isn't the result type",
    "function body isn't the result type",
    NULL};
  DO(test_expected_errors(src, "expr", errs));

  std::vector<std::string> notes = copy_notes();
  ASSERT_EQ(2U, notes.size());
  ASSERT_EQ("", notes[0]);
  ASSERT_EQ("in method 'f' inherited by C from T", notes[1]);
}

TEST_F(InheritedCopyTest, GuardedSpecializationCopy)
{
  // C's copy of the specialization of f whose guard holds for U8 reports its
  // body with the note.
  const char* src =
    "trait T[A: Any val]\n"
    "  fun f(): U8 => 0\n"
    "  fun f(): U8 iftype A <: U8 val => \"x\"\n"

    "class C is T[U8]\n";

  const char* errs[] = {
    "function body isn't the result type",
    "function body isn't the result type",
    NULL};
  DO(test_expected_errors(src, "expr", errs));

  std::vector<std::string> notes = copy_notes();
  ASSERT_EQ(2U, notes.size());
  ASSERT_EQ("", notes[0]);
  ASSERT_EQ("in method 'f' inherited by C from T", notes[1]);
}

TEST_F(InheritedCopyTest, DefaultArgumentOfCopy)
{
  // The default is checked where the trait declares it and in C's copy of
  // the method, with the note; the call that uses it reports nothing.
  const char* src =
    "trait T\n"
    "  fun f(x: U8 = \"s\"): U8 => x\n"

    "class C is T\n"

    "actor Main\n"
    "  new create(env: Env) => C.f()\n";

  const char* errs[] = {
    "default argument is not a subtype of the parameter type",
    "default argument is not a subtype of the parameter type",
    NULL};
  DO(test_expected_errors(src, "expr", errs));

  std::vector<std::string> notes = copy_notes();
  ASSERT_EQ(2U, notes.size());
  ASSERT_EQ("", notes[0]);
  ASSERT_EQ("in method 'f' inherited by C from T", notes[1]);
}

TEST_F(InheritedCopyTest, ClassFromLiteralInCopy)
{
  // C comes first, so the class made from the literal in C's copy of m is
  // checked first; its error ends the pass, and it carries the note.
  const char* src =
    "class C is T\n"

    "trait T\n"
    "  fun m(): Any val =>\n"
    "    let v: U8 = 1\n"
    "    object val fun k(): U8 => v.string() end\n";

  const char* errs[] = {"function body isn't the result type", NULL};
  DO(test_expected_errors(src, "expr", errs));

  std::vector<std::string> notes = copy_notes();
  ASSERT_EQ(1U, notes.size());
  ASSERT_EQ("in method 'm' inherited by C from T", notes[0]);
}

TEST_F(InheritedCopyTest, OwnCodeHasNoNote)
{
  // The same error in a class's own method, beside a method it inherits.
  const char* src =
    "trait T\n"
    "  fun g(): U8 => 0\n"

    "class C is T\n"
    "  fun f(): U8 => \"x\"\n";

  const char* errs[] = {"function body isn't the result type", NULL};
  DO(test_expected_errors(src, "expr", errs));

  std::vector<std::string> notes = copy_notes();
  ASSERT_EQ(1U, notes.size());
  ASSERT_EQ("", notes[0]);
}
