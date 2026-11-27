#include <gtest/gtest.h>
#include <platform.h>

#include <codegen/codegen.h>

#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>

#include <string.h>

#include <string>
#include <vector>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))

// Debug info for methods: a method whose body a trait supplied names the file
// and line of that trait's method, even when another trait supplied its
// signature.
class CodegenDebugInfoTest : public PassTest
{
protected:
  virtual void SetUp()
  {
    PassTest::SetUp();
    opt.release = false;
  }

  // The definition subprogram of the function named `name`.
  LLVMMetadataRef subprogram(const char* name)
  {
    LLVMValueRef fun = LLVMGetNamedFunction(compile->module, name);

    if(fun == NULL)
      return NULL;

    return LLVMGetSubprogram(fun);
  }

  // The file name a scope names.
  std::string file_of(LLVMMetadataRef scope)
  {
    unsigned len = 0;
    const char* name = LLVMDIFileGetFilename(LLVMDIScopeGetFile(scope), &len);
    return std::string(name, len);
  }

  // The lines of the debug locations of the instructions in a function.
  std::vector<unsigned> body_lines(const char* name)
  {
    std::vector<unsigned> lines;
    LLVMValueRef fun = LLVMGetNamedFunction(compile->module, name);

    for(LLVMBasicBlockRef b = LLVMGetFirstBasicBlock(fun); b != NULL;
      b = LLVMGetNextBasicBlock(b))
    {
      for(LLVMValueRef i = LLVMGetFirstInstruction(b); i != NULL;
        i = LLVMGetNextInstruction(i))
      {
        unsigned line = LLVMGetDebugLocLine(i);
        if(line != 0)
          lines.push_back(line);
      }
    }

    return lines;
  }
};

static bool ends_with(const std::string& s, const char* suffix)
{
  size_t len = strlen(suffix);
  return (s.size() >= len) && (s.compare(s.size() - len, len, suffix) == 0);
}

#define FIXTURE(name) PONY_TEST_FIXTURES_DIR "/debug-info/" name

TEST_F(CodegenDebugInfoTest, InheritedBodyNamesTheBodysFileAndLine)
{
  // Sig, in sig.pony, gives get's signature; Donor, in donor.pony, gives its
  // body, declared at line 10 with its statements at lines 11 and 12.
  add_module_path("sig", FIXTURE("sig"));
  add_module_path("donor", FIXTURE("donor"));

  const char* src =
    "use s = \"sig\"\n"
    "use d = \"donor\"\n"
    "class C is (s.Sig & d.Donor)\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.get()";

  TEST_COMPILE(src);

  LLVMMetadataRef sp = subprogram("C_box_get_W");
  ASSERT_TRUE(sp != NULL);
  ASSERT_TRUE(ends_with(file_of(sp), "donor.pony")) << file_of(sp);
  ASSERT_EQ(10u, LLVMDISubprogramGetLine(sp));

  std::vector<unsigned> lines = body_lines("C_box_get_W");
  ASSERT_FALSE(lines.empty());

  for(unsigned line : lines)
  {
    ASSERT_GE(line, 11u);
    ASSERT_LE(line, 12u);
  }
}

TEST_F(CodegenDebugInfoTest, InheritedMethodNamesItsTraitsFileAndLine)
{
  // Both, in both.pony, gives get's signature at line 9 and its body at lines
  // 10 and 11.
  add_module_path("both", FIXTURE("both"));

  const char* src =
    "use o = \"both\"\n"
    "class C is o.Both\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.get()";

  TEST_COMPILE(src);

  LLVMMetadataRef sp = subprogram("C_box_get_W");
  ASSERT_TRUE(sp != NULL);
  ASSERT_TRUE(ends_with(file_of(sp), "both.pony")) << file_of(sp);
  ASSERT_EQ(9u, LLVMDISubprogramGetLine(sp));

  std::vector<unsigned> lines = body_lines("C_box_get_W");
  ASSERT_FALSE(lines.empty());

  for(unsigned line : lines)
  {
    ASSERT_GE(line, 10u);
    ASSERT_LE(line, 11u);
  }
}

TEST_F(CodegenDebugInfoTest, InheritedSpecializationNamesItsFileAndLine)
{
  // Grouped, in group.pony, gives g a general body and, at line 9, a body for
  // U8 at lines 10 and 11. C's g for U8 names that file and line.
  add_module_path("group", FIXTURE("group"));

  const char* src =
    "use o = \"group\"\n"
    "class C is o.Grouped\n"
    "  new create() => None\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    C.g[U8](1)";

  TEST_COMPILE(src);

  // Each g C has is for U8; their names are generated from the type argument
  // and the receiver capability, so find them as the C functions with "_g_"
  // in their names.
  size_t found = 0;

  for(LLVMValueRef f = LLVMGetFirstFunction(compile->module); f != NULL;
    f = LLVMGetNextFunction(f))
  {
    size_t len = 0;
    std::string name(LLVMGetValueName2(f, &len), len);

    if((name.compare(0, 2, "C_") != 0) ||
      (name.find("_g_") == std::string::npos) ||
      (LLVMGetSubprogram(f) == NULL))
      continue;

    found++;
    LLVMMetadataRef sp = LLVMGetSubprogram(f);
    ASSERT_TRUE(ends_with(file_of(sp), "group.pony")) << name;
    ASSERT_EQ(9u, LLVMDISubprogramGetLine(sp)) << name;

    std::vector<unsigned> lines = body_lines(name.c_str());
    ASSERT_FALSE(lines.empty()) << name;

    for(unsigned line : lines)
    {
      ASSERT_GE(line, 10u) << name;
      ASSERT_LE(line, 11u) << name;
    }
  }

  ASSERT_GT(found, 0u);
}

TEST_F(CodegenDebugInfoTest, LiteralMethodNamesTheLiteralsFile)
{
  // The object literal in getter.pony declares get at line 20; its class's
  // own method names that file and line.
  add_module_path("getter", FIXTURE("getter"));

  const char* src =
    "use l = \"getter\"\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    l.Make().get()";

  TEST_COMPILE(src);

  // The literal's class has no fields, so its get is a `val` method; its name
  // is generated, so find it as the one `_val_get_W` function that isn't
  // Getter's.
  LLVMMetadataRef sp = NULL;

  for(size_t i = 0; i < compile->per_module_count; i++)
  {
    LLVMModuleRef module = compile->per_module_states[i].module;

    for(LLVMValueRef f = LLVMGetFirstFunction(module); f != NULL;
      f = LLVMGetNextFunction(f))
    {
      size_t len = 0;
      std::string name(LLVMGetValueName2(f, &len), len);

      if(ends_with(name, "_val_get_W") &&
        (name.find("Getter") == std::string::npos) &&
        (LLVMGetSubprogram(f) != NULL))
      {
        ASSERT_TRUE(sp == NULL) << "more than one get: " << name;
        sp = LLVMGetSubprogram(f);
      }
    }
  }

  ASSERT_TRUE(sp != NULL);
  ASSERT_TRUE(ends_with(file_of(sp), "getter.pony")) << file_of(sp);
  ASSERT_EQ(20u, LLVMDISubprogramGetLine(sp));
}
