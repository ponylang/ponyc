#include <gtest/gtest.h>
#include <llvm-c/Core.h>
#include <codegen/codegen.h>

class HostTest : public testing::Test
{
};

TEST_F(HostTest, ValidX86_64CpuAccepted)
{
  ASSERT_TRUE(codegen_host_cpu_is_valid("x86_64-unknown-linux-gnu", "x86-64"));
}

TEST_F(HostTest, X86_64CpuZnver3Accepted)
{
  ASSERT_TRUE(
    codegen_host_cpu_is_valid("x86_64-unknown-linux-gnu", "znver3"));
}

TEST_F(HostTest, ThirtyTwoBitOnlyCpuRejectedOnX86_64)
{
  ASSERT_FALSE(
    codegen_host_cpu_is_valid("x86_64-unknown-linux-gnu", "athlon-xp"));
}

TEST_F(HostTest, ThirtyTwoBitCpuAcceptedOnI686)
{
  ASSERT_TRUE(codegen_host_cpu_is_valid("i686-unknown-linux-gnu", "athlon-xp"));
}

TEST_F(HostTest, UnknownCpuRejected)
{
  ASSERT_FALSE(
    codegen_host_cpu_is_valid("x86_64-unknown-linux-gnu", "totally-bogus"));
}

TEST_F(HostTest, EmptyCpuRejected)
{
  ASSERT_FALSE(codegen_host_cpu_is_valid("x86_64-unknown-linux-gnu", ""));
}

TEST_F(HostTest, FilterFeaturesEmptyString)
{
  char* result = codegen_filter_host_features(
    "x86_64-unknown-linux-gnu", "");
  ASSERT_STREQ("", result);
  LLVMDisposeMessage(result);
}

TEST_F(HostTest, FilterFeaturesNullptr)
{
  char* result = codegen_filter_host_features(
    "x86_64-unknown-linux-gnu", nullptr);
  ASSERT_STREQ("", result);
  LLVMDisposeMessage(result);
}

TEST_F(HostTest, FilterFeaturesKeepsKnownFeatures)
{
  char* result = codegen_filter_host_features(
    "x86_64-unknown-linux-gnu", "+sse,+sse2,+cx16");
  ASSERT_STREQ("+sse,+sse2,+cx16", result);
  LLVMDisposeMessage(result);
}

TEST_F(HostTest, FilterFeaturesRemovesUnknown)
{
  char* result = codegen_filter_host_features(
    "x86_64-unknown-linux-gnu",
    "+sse,+totally_bogus_cpu_feature,+sse2");
  ASSERT_STREQ("+sse,+sse2", result);
  LLVMDisposeMessage(result);
}

TEST_F(HostTest, FilterFeaturesAllUnknownReturnsEmpty)
{
  char* result = codegen_filter_host_features(
    "x86_64-unknown-linux-gnu", "+bogus1,+bogus2");
  ASSERT_STREQ("", result);
  LLVMDisposeMessage(result);
}

TEST_F(HostTest, FilterFeaturesSingleKnownFeature)
{
  char* result = codegen_filter_host_features(
    "x86_64-unknown-linux-gnu", "+sse2");
  ASSERT_STREQ("+sse2", result);
  LLVMDisposeMessage(result);
}

TEST_F(HostTest, FilterFeaturesPreservesDisabledFeatures)
{
  char* result = codegen_filter_host_features(
    "x86_64-unknown-linux-gnu", "+sse,-sse2");
  ASSERT_STREQ("+sse,-sse2", result);
  LLVMDisposeMessage(result);
}
