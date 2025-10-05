#include <gtest/gtest.h>
#include <platform.h>
#include <string>

#include <ast/ast.h>
#include <ast/source.h>
#include <ast/token.h>
#include <pass/pass.h>
#include <pkg/package.h>
//#include <program/program.h>

// pass_opt_t is defined in ponyc/src/libponyc/pass/pass.h

#include "util.h"

using std::string;

#define TEST_COMPILE(src) DO(test_compile(src, "dartsrc"))
#define TEST_ERROR(src) DO(test_error(src, "dartsrc"))

class DartSrcTest : public PassTest
{};


TEST_F(DartSrcTest, HelloWorld)
{
  const char* src =
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    env.out.print(\"Hello, world!\")\n";

  //  _expected =
  //    "void main() {\n"
  //    "    print('Hello, world!');\n"
  //    "}\n";

  // TODO: get the output from the DARTSRC pass
  // into got_dart_src and compare it to _expected.
  // For now, just run the pass and assert true.
  //DO(test(src));
  
  TEST_COMPILE(src);
}

