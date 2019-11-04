#include <gtest/gtest.h>
#include <platform.h>

#include <pkg/package.h>
#include <pkg/program.h>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))

class SignatureTest : public PassTest
{};

TEST_F(SignatureTest, RecompilationSameSignature)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  char first_signature[SIGNATURE_LENGTH];

  TEST_COMPILE(src);

  const char* signature = program_signature(program);
  memcpy(first_signature, signature, SIGNATURE_LENGTH);

  TEST_COMPILE(src);

  signature = program_signature(program);

  ASSERT_TRUE(memcmp(first_signature, signature, SIGNATURE_LENGTH) == 0);
}

TEST_F(SignatureTest, PackageReferences)
{
  const char* pkg =
    "primitive Foo";

  const char* src1 =
    "use pkg1 = \"pkg1\"\n"
    "use pkg2 = \"pkg2\"\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    pkg1.Foo";

  const char* src2 =
    "use pkg2 = \"pkg2\"\n"
    "use pkg1 = \"pkg1\"\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    pkg2.Foo";

  char main1_signature[SIGNATURE_LENGTH];
  char pkg11_signature[SIGNATURE_LENGTH];
  char pkg21_signature[SIGNATURE_LENGTH];

  add_package("pkg1", pkg);
  add_package("pkg2", pkg);

  TEST_COMPILE(src1);

  ast_t* main_ast = ast_child(program);
  ast_t* pkg1_ast = ast_get(program, stringtab("pkg1"), nullptr);
  ast_t* pkg2_ast = ast_get(program, stringtab("pkg2"), nullptr);

  const char* signature = package_signature(main_ast);
  memcpy(main1_signature, signature, SIGNATURE_LENGTH);
  signature = package_signature(pkg1_ast);
  memcpy(pkg11_signature, signature, SIGNATURE_LENGTH);
  signature = package_signature(pkg2_ast);
  memcpy(pkg21_signature, signature, SIGNATURE_LENGTH);

  TEST_COMPILE(src2);

  main_ast = ast_child(program);
  pkg1_ast = ast_get(program, stringtab("pkg1"), nullptr);
  pkg2_ast = ast_get(program, stringtab("pkg2"), nullptr);

  signature = package_signature(main_ast);

  ASSERT_FALSE(memcmp(main1_signature, signature, SIGNATURE_LENGTH) == 0);

  signature = package_signature(pkg1_ast);

  ASSERT_TRUE(memcmp(pkg11_signature, signature, SIGNATURE_LENGTH) == 0);

  signature = package_signature(pkg2_ast);

  ASSERT_TRUE(memcmp(pkg21_signature, signature, SIGNATURE_LENGTH) == 0);
}

TEST_F(SignatureTest, DocstringIgnored)
{
  const char* src1 =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    None";

  const char* src2 =
    "actor Main\n"
    "  \"\"\"Foo\"\"\"\n"
    "  new create(env: Env) =>\n"
    "    None";

  const char* src3 =
    "actor Main\n"
    "  \"\"\"Bar\"\"\"\n"
    "  new create(env: Env) =>\n"
    "    None";

  char first_signature[SIGNATURE_LENGTH];

  TEST_COMPILE(src1);

  const char* signature = program_signature(program);
  memcpy(first_signature, signature, SIGNATURE_LENGTH);

  TEST_COMPILE(src2);

  signature = program_signature(program);

  ASSERT_TRUE(memcmp(first_signature, signature, SIGNATURE_LENGTH) == 0);

  TEST_COMPILE(src3);

  signature = program_signature(program);

  ASSERT_TRUE(memcmp(first_signature, signature, SIGNATURE_LENGTH) == 0);
}

extern "C"
{

static char sig_in[SIGNATURE_LENGTH];

EXPORT_SYMBOL char* signature_get()
{
  return sig_in;
}

}

TEST_F(SignatureTest, SerialiseSignature)
{
  const char* src =
    "use \"serialise\"\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    let sig_in = @signature_get[Pointer[U8]]()\n"
    "    let sig = Serialise.signature()\n"
    "    let ok = @memcmp[I32](sig_in, sig.cpointer(), sig.size())\n"
    "    if ok == 0 then\n"
    "      @pony_exitcode[None](I32(1))\n"
    "    end";

  set_builtin(nullptr);

  TEST_COMPILE(src);

  const char* signature = program_signature(program);
  memcpy(sig_in, signature, SIGNATURE_LENGTH);

  int exit_code = 0;
  ASSERT_TRUE(run_program(&exit_code));
  ASSERT_EQ(exit_code, 1);
}
