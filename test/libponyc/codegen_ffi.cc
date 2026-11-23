#include <gtest/gtest.h>
#include <platform.h>
#include <codegen/codegen.h>
#include <cstring>

#include "util.h"

#define TEST_COMPILE(src) DO(test_compile(src, "ir"))

#define TEST_ERRORS_1(src, err1) \
  { const char* errs[] = {err1, NULL}; \
    DO(test_expected_errors(src, "ir", errs)); }


class CodegenFFITest : public PassTest
{};

struct FFIExtensionTarget
{
  const char* triple;
  bool riscv;
  bool rv64;
};

class CodegenFFIExtensionTest : public CodegenFFITest,
  public testing::WithParamInterface<FFIExtensionTarget>
{
protected:
  void SetUp() override
  {
    CodegenFFITest::SetUp();
    codegen_pass_cleanup(&opt);
    opt.triple = const_cast<char*>(GetParam().triple);
    ASSERT_TRUE(codegen_pass_init(&opt));
  }

  void check_extensions(const char* name, const char* extensions,
    unsigned call_count, unsigned variadic_call_count = 0)
  {
    SCOPED_TRACE(name);
    LLVMValueRef function = LLVMGetNamedFunction(compile->module, name);
    ASSERT_NE(function, nullptr);
    unsigned zeroext = LLVMGetEnumAttributeKindForName("zeroext", 7);
    unsigned signext = LLVMGetEnumAttributeKindForName("signext", 7);
    unsigned param_count = LLVMCountParams(function);
    ASSERT_EQ(std::strlen(extensions), param_count + 1);

    for(unsigned index = 0; index <= param_count; index++)
    {
      SCOPED_TRACE(index);
      EXPECT_EQ(LLVMGetEnumAttributeAtIndex(function, index, zeroext) != nullptr,
        extensions[index] == 'z');
      EXPECT_EQ(LLVMGetEnumAttributeAtIndex(function, index, signext) != nullptr,
        extensions[index] == 's');
    }

    unsigned calls = 0;
    unsigned variadic_calls = 0;
    for(LLVMUseRef use = LLVMGetFirstUse(function); use != nullptr;
      use = LLVMGetNextUse(use))
    {
      LLVMValueRef call = LLVMGetUser(use);
      if(LLVMIsACallInst(call) == nullptr || LLVMGetCalledValue(call) != function)
        continue;

      calls++;
      unsigned arg_count = LLVMGetNumArgOperands(call);
      if(arg_count > param_count)
      {
        variadic_calls++;
        ASSERT_EQ(arg_count, param_count + 1);
        EXPECT_EQ(LLVMTypeOf(LLVMGetOperand(call, param_count)), compile->i8);
      }
      for(unsigned index = 0; index <= arg_count; index++)
      {
        SCOPED_TRACE(index);
        char extension = index <= param_count ? extensions[index] : '-';
        EXPECT_EQ(LLVMGetCallSiteEnumAttribute(call, index, zeroext) != nullptr,
          extension == 'z');
        EXPECT_EQ(LLVMGetCallSiteEnumAttribute(call, index, signext) != nullptr,
          extension == 's');
      }
    }
    EXPECT_EQ(calls, call_count);
    EXPECT_EQ(variadic_calls, variadic_call_count);
  }
};

INSTANTIATE_TEST_SUITE_P(Target, CodegenFFIExtensionTest, testing::Values(
  FFIExtensionTarget{"riscv32-unknown-linux-gnu", true, false},
  FFIExtensionTarget{"riscv64-unknown-linux-gnu", true, true},
  FFIExtensionTarget{"x86_64-unknown-linux-gnu", false, false}));

TEST_P(CodegenFFIExtensionTest, FixedIntegerParametersAndResults)
{
  const char* src =
    "use @ffi_scalar[U16](a: U8, b: I8, c: U16, d: I16, e: Bool, "
      "f: U32, g: I32, h: U64, ...)\n"
    "use @ffi_signed[I16]()\n"
    "use @ffi_width_override[U16]()\n"
    "use @\"llvm.bswap.i16\"[U16](value: U16)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @ffi_scalar[I16](1, -2, 3, -4, true, 5, -6, 7, U8(255))\n"
    "    @ffi_scalar(1, -2, 3, -4, false, 5, -6, 7)\n"
    "    @ffi_signed()\n"
    "    @ffi_width_override[U8]()\n"
    "    @\"llvm.bswap.i16\"(U16(0x1234))";

  TEST_COMPILE(src);
  const auto& target = GetParam();
  const char* scalar_extensions = "---------";
  if(target.riscv)
    scalar_extensions = target.rv64 ? "zzszszss-" : "zzszsz---";

  check_extensions("ffi_scalar", scalar_extensions, 2, 1);
  check_extensions("ffi_signed", target.riscv ? "s" : "-", 1);
  check_extensions("ffi_width_override", "-", 1);
  check_extensions("llvm.bswap.i16", "--", 1);
}

TEST_P(CodegenFFIExtensionTest, ConflictingParameterExtensions)
{
  add_module("pkg",
    "use @ffi_scalar[None](x: I16)\n"
    "primitive Foreign\n"
    "  fun apply() => @ffi_scalar(-1)");

  const char* src =
    "use \"pkg\"\n"
    "use @ffi_scalar[None](x: U16)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @ffi_scalar(1)\n"
    "    Foreign()";

  if(GetParam().riscv)
  {
    TEST_ERRORS_1(src,
      "conflicting declarations for FFI function: incompatible RISC-V "
      "integer extensions");
  } else {
    TEST_COMPILE(src);
  }
}

TEST_P(CodegenFFIExtensionTest, ConflictingReturnExtensions)
{
  add_module("pkg",
    "use @ffi_scalar[I16]()\n"
    "primitive Foreign\n"
    "  fun apply(): I16 => @ffi_scalar()");

  const char* src =
    "use \"pkg\"\n"
    "use @ffi_scalar[U16]()\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @ffi_scalar()\n"
    "    Foreign()";

  if(GetParam().riscv)
  {
    TEST_ERRORS_1(src,
      "conflicting declarations for FFI function: incompatible RISC-V "
      "integer extensions");
  } else {
    TEST_COMPILE(src);
  }
}

TEST_P(CodegenFFIExtensionTest, CompatibleIntegerExtensions)
{
  add_module("pkg",
    "use @ffi_scalar[I32](x: I32)\n"
    "primitive Foreign\n"
    "  fun apply(): I32 => @ffi_scalar(-1)");

  const char* src =
    "use \"pkg\"\n"
    "use @ffi_scalar[U32](x: U32)\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @ffi_scalar(1)\n"
    "    Foreign()";

  TEST_COMPILE(src);
  check_extensions("ffi_scalar", GetParam().rv64 ? "ss" : "--", 1);
}


TEST_F(CodegenFFITest, ReferenceReferenceCompatibility)
{
  const char* pkg =
    "use @foo[None](x: Foo)\n"

    "primitive Foo\n"
    "  fun apply() => @foo(this)";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[None](x: Main)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(this)\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_COMPILE(src);
}


TEST_F(CodegenFFITest, ReferencePointerCompatibility)
{
  const char* pkg =
    "use @foo[None](x: Foo)\n"

    "primitive Foo\n"
    "  fun apply() => @foo(this)";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[None](x: Pointer[Main])\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(Pointer[Main])\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_COMPILE(src);
}


TEST_F(CodegenFFITest, ReferenceUSizeCompatibility)
{
  const char* pkg =
    "use @foo[None](x: Foo)\n"

    "primitive Foo\n"
    "  fun apply() => @foo(this)";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[None](x: USize)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(0)\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_COMPILE(src);
}


TEST_F(CodegenFFITest, PointerPointerCompatibility)
{
  const char* pkg =
    "use @foo[None](x: Pointer[Foo])\n"

    "primitive Foo\n"
    "  fun apply() => @foo(Pointer[Foo])";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[None](x: Pointer[Main])\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(Pointer[Main])\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_COMPILE(src);
}


TEST_F(CodegenFFITest, PointerUSizeCompatibility)
{
  const char* pkg =
    "use @foo[None](x: Pointer[Foo])\n"

    "primitive Foo\n"
    "  fun apply() => @foo(Pointer[Foo])";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[None](x: USize)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(0)\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_COMPILE(src);
}


TEST_F(CodegenFFITest, RuntimeCall)
{
  const char* src =
    "use @pony_ctx[Pointer[None]]()\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @pony_ctx()";

  TEST_COMPILE(src);
}



TEST_F(CodegenFFITest, DeclParameterCountConflict)
{
  const char* pkg =
    "use @foo[None](x: U32)\n"

    "primitive Foo\n"
    "  fun apply() => @foo(0)";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[None](x: U32, y: U32)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(0, 0)\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_ERRORS_1(src, "conflicting declarations for FFI function: declarations "
    "have an incompatible number of parameters");
}


TEST_F(CodegenFFITest, DeclParameterTypeConflict)
{
  const char* pkg =
    "use @foo[None](x: U32)\n"

    "primitive Foo\n"
    "  fun apply() => @foo(0)";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[None](x: U64)\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo(0)\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_ERRORS_1(src, "conflicting calls for FFI function: parameters "
    "have incompatible types");
}


TEST_F(CodegenFFITest, DeclReturnTypeConflict)
{
  const char* pkg =
    "use @foo[U32]()\n"

    "primitive Foo\n"
    "  fun apply() => @foo()";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[U64]()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo()\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_ERRORS_1(src, "conflicting calls for FFI function: return values "
    "have incompatible types");
}

TEST_F(CodegenFFITest, CallReturnTypeConflict)
{
  const char* src =
    "use @foo[U64]()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo()\n"
    "    @foo[U32]()";


  TEST_ERRORS_1(src, "conflicting calls for FFI function: return values "
    "have incompatible types");
}


TEST_F(CodegenFFITest, DeclReturnTypeVoidConflict)
{
  const char* pkg =
    "use @foo[None]()\n"

    "primitive Foo\n"
    "  fun apply() => @foo()";

  const char* src =
    "use \"pkg\"\n"
    "use @foo[Main]()\n"

    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @foo()\n"
    "    Foo()";

  add_module("pkg", pkg);

  TEST_ERRORS_1(src, "conflicting calls for FFI function: return values "
    "have incompatible types");
}


TEST_F(CodegenFFITest, InternalABIFunctionConflict)
{
  const char* src =
    "use @Main_Dispatch[None]()\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @Main_Dispatch()";

  TEST_ERRORS_1(src, "cannot use 'Main_Dispatch' as an FFI name: name is "
    "already in use by the internal ABI");
}


TEST_F(CodegenFFITest, InternalABIObjectConflict)
{
  const char* src =
    "use @Main_Desc[None]()\n"
    "actor Main\n"
    "  new create(env: Env) =>\n"
    "    @Main_Desc()";

  TEST_ERRORS_1(src, "cannot use 'Main_Desc' as an FFI name: name is already "
    "in use by the internal ABI");
}
