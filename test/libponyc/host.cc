#include <gtest/gtest.h>
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
