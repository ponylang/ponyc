#include <gtest/gtest.h>
#include <platform.h>

#include "../../../src/libponyrt/dbg/dbg_util.h"

#define DBG_ENABLED true
#include "../../../src/libponyrt/dbg/dbg.h"

/**
 * Example of bits defined by a base_name and offset
 * xxxx_bit_idx where base name is xxxx and xxxx_bit_idx
 * is the offset for the first bit for base name in the
 * bits array. xxxx_bit_cnt is the number of bit reserved
 * for xxxx.
 */
enum {
  DBG_BITS_FIRST(first, 30),
  DBG_BITS_NEXT(second, 3, first),
  DBG_BITS_NEXT(another, 2, second),
  DBG_BITS_SIZE(bit_count, another)
};

class DbgTest : public testing::Test
{};

TEST_F(DbgTest, DbgDoEmpty)
{
  // Should compile
  _DBG_DO();
}

TEST_F(DbgTest, DbgDoSingleStatement)
{
  char buffer[10];
  _DBG_DO(strcpy(buffer, "Hi"));
  EXPECT_STREQ(buffer, "Hi");
}

TEST_F(DbgTest, DbgDoMultiStatement)
{
  char buffer[10];
  _DBG_DO(strcpy(buffer, "Hi"); strcat(buffer, ", Bye"));
  EXPECT_STREQ(buffer, "Hi, Bye");
}

TEST_F(DbgTest, DbgBitsArrayIdx)
{
  EXPECT_EQ(_DBG_BITS_ARRAY_IDX(0), 0);
  EXPECT_EQ(_DBG_BITS_ARRAY_IDX(31), 0);
  EXPECT_EQ(_DBG_BITS_ARRAY_IDX(32), 1);
  EXPECT_EQ(_DBG_BITS_ARRAY_IDX(63), 1);
  EXPECT_EQ(_DBG_BITS_ARRAY_IDX(64), 2);
  EXPECT_EQ(_DBG_BITS_ARRAY_IDX(95), 2);
}

TEST_F(DbgTest, DbgBitMask)
{
  EXPECT_EQ(_DBG_BIT_MASK(0),  0x00000001);
  EXPECT_EQ(_DBG_BIT_MASK(1),  0x00000002);
  EXPECT_EQ(_DBG_BIT_MASK(31), 0x80000000);
  EXPECT_EQ(_DBG_BIT_MASK(32), 0x00000001);
  EXPECT_EQ(_DBG_BIT_MASK(63), 0x80000000);
  EXPECT_EQ(_DBG_BIT_MASK(64), 0x00000001);
  EXPECT_EQ(_DBG_BIT_MASK(95), 0x80000000);
}

TEST_F(DbgTest, DbgBnoi)
{
  EXPECT_EQ(dbg_bnoi(first,0), 0);
  EXPECT_EQ(dbg_bnoi(first,1), 1);
  EXPECT_EQ(dbg_bnoi(second,0), 30);
  EXPECT_EQ(dbg_bnoi(second,1), 31);
  EXPECT_EQ(dbg_bnoi(second,2), 32);
  EXPECT_EQ(dbg_bnoi(another,0), 33);
  EXPECT_EQ(dbg_bnoi(another,1), 34);
  EXPECT_EQ(bit_count, 30 + 3 + 2);
}

TEST_F(DbgTest, DbgReadWriteBitsOfSecond)
{
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_file(bit_count, stderr);

  // Get bit index of second[0] and second[1] they should be adjacent
  uint32_t bi0 = dbg_bnoi(second, 0);
  uint32_t bi1 = dbg_bnoi(second, 1);
  EXPECT_EQ(bi0 + 1, bi1);

  // Initially they should be zero
  bool o0 = dbg_gb(dc, bi0);
  bool o1 = dbg_gb(dc, bi1);
  EXPECT_FALSE(o0);
  EXPECT_FALSE(o1);

  // Write new values and verify they changed
  dbg_sb(dc, bi0, !o0);
  dbg_sb(dc, bi1, !o1);
  EXPECT_EQ(dbg_gb(dc, bi0), !o0);
  EXPECT_EQ(dbg_gb(dc, bi1), !o1);

  // Restore original values
  dbg_sb(dc, bi0, o0);
  dbg_sb(dc, bi1, o1);
  EXPECT_EQ(dbg_gb(dc, bi0), o0);
  EXPECT_EQ(dbg_gb(dc, bi1), o1);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgCreateDestroy)
{
  // Even with 0 bits we still create at least 1 bit
  dbg_ctx_t* dc = dbg_ctx_create(0);
  ASSERT_TRUE(dc->bits != NULL);
  EXPECT_EQ(dc->bits[0], 0);
  EXPECT_TRUE(dc->dst_file == NULL);
  EXPECT_TRUE(dc->dst_buf == NULL);
  EXPECT_EQ(dc->dst_buf_size, 0);
  dbg_ctx_destroy(dc);

  dc = dbg_ctx_create(1);
  ASSERT_TRUE(dc->bits != NULL);
  EXPECT_EQ(dc->bits[0], 0);
  EXPECT_TRUE(dc->dst_file == NULL);
  EXPECT_TRUE(dc->dst_buf == NULL);
  EXPECT_EQ(dc->dst_buf_size, 0);
  dbg_ctx_destroy(dc);

  dc = dbg_ctx_create_with_dst_file(1, stdout);
  ASSERT_TRUE(dc->bits != NULL);
  EXPECT_EQ(dc->bits[0], 0);
  EXPECT_TRUE(dc->dst_file == stdout);
  EXPECT_TRUE(dc->dst_buf == NULL);
  EXPECT_EQ(dc->dst_buf_size, 0);
  dbg_ctx_destroy(dc);

  // Verify dst_buf and tmp_buf are writeable
  dc = dbg_ctx_create_with_dst_buf(1, 0x1000, 0x100);
  ASSERT_TRUE(dc->bits != NULL);
  EXPECT_EQ(dc->bits[0], 0);
  EXPECT_TRUE(dc->dst_file == NULL);
  EXPECT_TRUE(dc->dst_buf != NULL);
  EXPECT_EQ(dc->dst_buf_size, 0x1000);
  for(size_t i = 0; i < dc->dst_buf_size; i++)
    EXPECT_EQ(dc->dst_buf[i], 0);
  memset(dc->dst_buf, 0xAA, dc->dst_buf_size);
  for(size_t i = 0; i < dc->dst_buf_size; i++)
    EXPECT_EQ(dc->dst_buf[i], 0xAA);
  memset(dc->dst_buf, 0x55, dc->dst_buf_size);
  for(size_t i = 0; i < dc->dst_buf_size; i++)
    EXPECT_EQ(dc->dst_buf[i], 0x55);
  memset(dc->dst_buf, 0x0, dc->dst_buf_size);
  for(size_t i = 0; i < dc->dst_buf_size; i++)
    EXPECT_EQ(dc->dst_buf[i], 0);

  // Verify tmp_buf is an extra byte in length
  EXPECT_TRUE(dc->tmp_buf != NULL);
  EXPECT_EQ(dc->tmp_buf_size, 0x100);
  for(size_t i = 0; i < dc->tmp_buf_size + 1; i++)
    EXPECT_EQ(dc->tmp_buf[i], 0);
  memset(dc->tmp_buf, 0xAA, dc->tmp_buf_size + 1);
  for(size_t i = 0; i < dc->tmp_buf_size + 1; i++)
    EXPECT_EQ(dc->tmp_buf[i], 0xAA);
  memset(dc->tmp_buf, 0x55, dc->tmp_buf_size + 1);
  for(size_t i = 0; i < dc->tmp_buf_size + 1; i++)
    EXPECT_EQ(dc->tmp_buf[i], 0x55);
  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgCtxBitsInitToZero)
{
  const uint32_t num_bits = 65;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_file(num_bits, stderr);

  // Verify all bits are zero
  for(uint32_t bi = 0; bi < num_bits; bi++)
  {
    EXPECT_FALSE(dbg_gb(dc, bi));
  }

  // Verify bits array is zero
  for(uint32_t i = 0; i < (num_bits + 31) / 32; i++)
  {
    EXPECT_EQ(dc->bits[i], 0);
  }

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, WalkingOneBit)
{
  const uint32_t num_bits = 29;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_file(num_bits, stderr);

  // Walking one bit
  for(uint32_t bi = 0; bi < num_bits; bi++)
  {
    bool v = dbg_gb(dc, bi);
    EXPECT_FALSE(v);
    dbg_sb(dc, bi, 1);
    v = dbg_gb(dc, bi);
    EXPECT_TRUE(v);
    
    // Verify only one bit is set
    for(uint32_t cbi = 0; cbi < num_bits; cbi++)
    {
      bool v = dbg_gb(dc, cbi);
      if (cbi == bi)
        EXPECT_TRUE(v);
      else
        EXPECT_FALSE(v);
    }

    // Verify only one bit is set using bits array
    uint32_t bits_array_idx = bi / 32;
    uint32_t bits_mask = 1 << (bi % 32);
    for(uint32_t i = 0; i < (num_bits + 31) / 32; i++)
    {
      if(i == bits_array_idx)
        EXPECT_EQ(dc->bits[i], bits_mask);
      else
        EXPECT_EQ(dc->bits[i], 0);
    }
    dbg_sb(dc, bi, 0);
  }

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, WalkingTwoBits)
{
  const uint32_t num_bits = 147;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_file(num_bits, stderr);

  // Number bits must be >= 2
  EXPECT_TRUE(num_bits >= 2);

  // Walking two bits
  dbg_sb(dc, 0, 1);
  for(uint32_t bi = 1; bi < num_bits - 1; bi++)
  {
    // Get first of the pair it should be 1
    bool v = dbg_gb(dc, bi - 1);
    EXPECT_TRUE(v);

    // Get second bit it should be 0
    v = dbg_gb(dc, bi);
    EXPECT_FALSE(v);
    
    // Set second bit
    dbg_sb(dc, bi, 1);

    // Verify two bits are set
    for(uint32_t cbi = 0; cbi < num_bits; cbi++)
    {
      bool v = dbg_gb(dc, cbi);
      if (cbi == (bi - 1))
        EXPECT_TRUE(v);
      else if (cbi == bi)
        EXPECT_TRUE(v);
      else
        EXPECT_FALSE(v);
    }
    
    // Clear first bit
    dbg_sb(dc, bi - 1, 0);
  }

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPsuOneByteBufWrite)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 1, 0x100);

  // Write "a"
  DBG_PSU(dc, "a");

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPsuOneByteBufWriteWrite)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 1, 0x100);

  // Write "a", "b"
  DBG_PSU(dc, "a");
  DBG_PSU(dc, "b");

  // Read and verify "b" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("b", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPsuOneByteBufWriteWriteWrite)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 1, 0x100);

  // Write "a", "b", "c"
  DBG_PSU(dc, "a");
  DBG_PSU(dc, "b");
  DBG_PSU(dc, "c");

  // Read and verify "b" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("c", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPsuOneByteBufWrite2)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 1, 0x100);

  // Write "ab"
  DBG_PSU(dc, "ab");

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPsuOneByteBufWrite3)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 1, 0x100);

  // Write "abc"
  DBG_PSU(dc, "abc");

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuOneByteBufWriteEqTmpBufSize)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 1, 5);

  // Write "abcde"
  const char* str = "abcde";
  ASSERT_EQ(strlen(str), 5);
  DBG_PFU(dc, "%s", str);

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuOneByteBufWriteGtTmpBufSize)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 1, 5);

  // Write "abcdef"
  const char* str = "abcdef";
  ASSERT_GT(strlen(str), 5);
  DBG_PFU(dc, "%s", str);

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufWriteReadRead)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "a"
  DBG_PFU(dc, "%s", "a");

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufWriteReadWriteReadWriteRead)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "a"
  DBG_PFU(dc, "%s", "a");

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Write "b"
  DBG_PFU(dc, "%s", "b");

  // Read and verify "b" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("b", read_buf);

  // Write "c"
  DBG_PFU(dc, "%s", "c");

  // Read and verify "c" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("c", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufFillEmpty)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "a" then "b"
  DBG_PFU(dc, "%s", "a");
  DBG_PFU(dc, "%s", "b");

  // Read verify "a"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 1);
  ASSERT_STREQ("a", read_buf);

  // Read verify "b"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 1);
  ASSERT_STREQ("b", read_buf);

  // Read verify ""
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 0);
  ASSERT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufOverFillBy1Empty)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "a", "b", "c"
  DBG_PFU(dc, "%s", "a");
  DBG_PFU(dc, "%s", "b");
  DBG_PFU(dc, "%s", "c");

  // Read verify "b"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 1);
  ASSERT_STREQ("b", read_buf);

  // Read verify "c"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 1);
  ASSERT_STREQ("c", read_buf);

  // Read verify ""
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 0);
  ASSERT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwByteBufOverFillBy2ReadTillEmpty)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "a", "b", "c", "d"
  DBG_PFU(dc, "%s", "a");
  DBG_PFU(dc, "%s", "b");
  DBG_PFU(dc, "%s", "c");
  DBG_PFU(dc, "%s", "d");

  // Read verify "c"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 1);
  ASSERT_STREQ("c", read_buf);

  // Read verify "d"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 1);
  ASSERT_STREQ("d", read_buf);

  // Read verify ""
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  ASSERT_EQ(cnt, 0);
  ASSERT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufFillSingleOpReadTillEmpty)
{
  char read_buf[3];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "ab"
  DBG_PFU(dc, "%s", "ab");

  // Read verify "ab"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  ASSERT_EQ(cnt, 2);
  ASSERT_STREQ("ab", read_buf);

  // Read verify ""
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  ASSERT_EQ(cnt, 0);
  ASSERT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufOverFillBy1SingleOpReadTillEmpty)
{
  char read_buf[3];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "ab"
  DBG_PFU(dc, "%s", "abc");

  // Read verify "ab"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  ASSERT_EQ(cnt, 2);
  ASSERT_STREQ("ab", read_buf);

  // Read verify ""
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  ASSERT_EQ(cnt, 0);
  ASSERT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufWrite2Read2)
{
  char read_buf[3];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "ab"
  const char* str = "ab";
  DBG_PFU(dc, "%s", str);

  // Read and verify "ab" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 2);
  EXPECT_STREQ("ab", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufWriteEqTmpBufSize)
{
  char read_buf[3];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 5);

  // Write "abcde"
  const char* str = "abcde";
  ASSERT_EQ(strlen(str), 5);
  DBG_PFU(dc, "%s", str);

  // Read and verify "ab" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 2);
  EXPECT_STREQ("ab", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufWriteGtTmpBufSize)
{
  char read_buf[3];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 5);

  // Write "abcdef"
  const char* str = "abcdef";
  ASSERT_GT(strlen(str), 5);
  DBG_PFU(dc, "%s", str);

  // Read and verify "ab" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 2);
  EXPECT_STREQ("ab", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufWriteWriteReadRead)
{
  char read_buf[2];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "a", "b"
  DBG_PFU(dc, "%s", "a");
  DBG_PFU(dc, "%s", "b");

  // Read and verify "a" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Write "c"
  DBG_PFU(dc, "%s", "c");

  // Read and verify "b" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("b", read_buf);

  // Read and verify "c" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("c", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuTwoByteBufWriteReadWrite2Read2)
{
  char read_buf[3];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 2, 0x100);

  // Write "a"
  DBG_PFU(dc, "%s", "a");

  // Read and verify "a"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 1);
  EXPECT_EQ(cnt, 1);
  EXPECT_STREQ("a", read_buf);

  // Write "bc"
  DBG_PFU(dc, "%s", "bc");

  // Read and verify "bc"
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 2);
  EXPECT_STREQ("bc", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfuThreeByteBufWrite3Read2WriteRead2)
{
  char read_buf[3];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 3, 0x100);

  // Write "abc"
  DBG_PFU(dc, "%s", "abc");

  // Read and verify "ab" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 2);
  //EXPECT_STREQ("ab", read_buf);
  EXPECT_STREQ("ab", read_buf);

  // Write "d"
  DBG_PFU(dc, "%s", "d");

  // Read verify "cd" was written
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 2);
  //EXPECT_STREQ("cd", read_buf);
  EXPECT_STREQ("cd", read_buf);

  // Read it again, it should now be empty
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), 2);
  EXPECT_EQ(cnt, 0);
  EXPECT_STREQ("", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfnu)
{
  char read_buf[0x100];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 0x100, 0x100);

  // Write
  DBG_PFNU(dc, "Yo %s\n", "Dude");

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
#ifndef PLATFORM_IS_WINDOWS
  EXPECT_EQ(cnt, 19);
  EXPECT_STREQ("TestBody:  Yo Dude\n", read_buf);
#else
  EXPECT_EQ(cnt, 41);
  EXPECT_STREQ("DbgTest_DbgPfnu_Test::TestBody:  Yo Dude\n", read_buf);
#endif

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPsnu)
{
  char read_buf[0x100];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 0x100, 0x100);

  // Write
  DBG_PSNU(dc, "hi\n");

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
#ifndef PLATFORM_IS_WINDOWS
  EXPECT_EQ(cnt, 14);
  EXPECT_STREQ("TestBody:  hi\n", read_buf);
#else
  EXPECT_EQ(cnt, 36);
  EXPECT_STREQ("DbgTest_DbgPsnu_Test::TestBody:  hi\n", read_buf);
#endif

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPf)
{
  char read_buf[0x100];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 0x100, 0x100);

  // Set bit 0 and Write
  dbg_sb(dc, 0, 1);
  DBG_PF(dc, 0, "hi %s", "there");

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
  EXPECT_EQ(cnt, 8);
  EXPECT_STREQ("hi there", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPs)
{
  char read_buf[0x100];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 0x100, 0x100);

  // Set bit 0 and Write
  dbg_sb(dc, 0, 1);
  DBG_PS(dc, 0, "hi there");

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
  EXPECT_EQ(cnt, 8);
  EXPECT_STREQ("hi there", read_buf);

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPfn)
{
  char read_buf[0x100];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 0x100, 0x100);

  // Set bit 0 and Write
  dbg_sb(dc, 0, 1);
  DBG_PFN(dc, 0, "hi %s", "there");

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
#ifndef PLATFORM_IS_WINDOWS
  EXPECT_EQ(cnt, 19);
  EXPECT_STREQ("TestBody:  hi there", read_buf);
#else
  EXPECT_EQ(cnt, 40);
  EXPECT_STREQ("DbgTest_DbgPfn_Test::TestBody:  hi there", read_buf);
#endif

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgPsn)
{
  char read_buf[0x100];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 0x100, 0x100);

  // Set bit 0 and Write
  dbg_sb(dc, 0, 1);
  DBG_PSN(dc, 0, "hi there");

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
#ifndef PLATFORM_IS_WINDOWS
  EXPECT_EQ(cnt, 19);
  EXPECT_STREQ("TestBody:  hi there", read_buf);
#else
  EXPECT_EQ(cnt, 40);
  EXPECT_STREQ("DbgTest_DbgPsn_Test::TestBody:  hi there", read_buf);
#endif

  dbg_ctx_destroy(dc);
}

TEST_F(DbgTest, DbgEX)
{
  char read_buf[0x100];
  size_t cnt;
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_buf(1, 0x100, 0x100);

  // Set bit 0 and Write
  dbg_sb(dc, 0, 1);

  DBG_E(dc, 0);
  DBG_X(dc, 0);

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
#ifndef PLATFORM_IS_WINDOWS
  EXPECT_EQ(cnt, 22);
  EXPECT_STREQ("TestBody:+\nTestBody:-\n", read_buf);
#else
  EXPECT_EQ(cnt, 62);
  EXPECT_STREQ("DbgTest_DbgEX_Test::TestBody:+\nDbgTest_DbgEX_Test::TestBody:-\n", read_buf);
#endif

  DBG_PFE(dc, 0, "Entered %d\n", 1);
  DBG_PFX(dc, 0, "Exited  %d\n", 2);

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
#ifndef PLATFORM_IS_WINDOWS
  EXPECT_EQ(cnt, 42);
  EXPECT_STREQ("TestBody:+ Entered 1\nTestBody:- Exited  2\n", read_buf);
#else
  EXPECT_EQ(cnt, 82);
  EXPECT_STREQ("DbgTest_DbgEX_Test::TestBody:+ Entered 1\nDbgTest_DbgEX_Test::TestBody:- Exited  2\n", read_buf);
#endif

  DBG_PSE(dc, 0, "Entered 1\n");
  DBG_PSX(dc, 0, "Exited  2\n");

  // Check it
  cnt = dbg_read(dc, read_buf, sizeof(read_buf), sizeof(read_buf)-1);
#ifndef PLATFORM_IS_WINDOWS
  EXPECT_EQ(cnt, 42);
  EXPECT_STREQ("TestBody:+ Entered 1\nTestBody:- Exited  2\n", read_buf);
#else
  EXPECT_EQ(cnt, 82);
  EXPECT_STREQ("DbgTest_DbgEX_Test::TestBody:+ Entered 1\nDbgTest_DbgEX_Test::TestBody:- Exited  2\n", read_buf);
#endif

  dbg_ctx_destroy(dc);
}

#if !defined(PLATFORM_IS_WINDOWS) && !defined(PLATFORM_IS_MACOSX)
TEST_F(DbgTest, DbgPsnuViaDstFileUsingFmemopen)
{
  char buffer[0x100];
  FILE* memfile = fmemopen(buffer, sizeof(buffer), "w+");
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_file(1, memfile);
  ASSERT_TRUE(dc != NULL);

  DBG_PSNU(dc, "hi\n");

  fclose(memfile);
  EXPECT_STREQ("TestBody:  hi\n", buffer);

  dbg_ctx_destroy(dc);
}
#endif

#if !defined(PLATFORM_IS_WINDOWS) && !defined(PLATFORM_IS_MACOSX)
TEST_F(DbgTest, DbgPfnuViaDstFileUsingFmemopen)
{
  char buffer[0x100];
  FILE* memfile = fmemopen(buffer, sizeof(buffer), "w+");
  dbg_ctx_t* dc = dbg_ctx_create_with_dst_file(1, memfile);

  DBG_PFNU(dc, "Yo %s\n", "Dude");

  fclose(memfile);
  EXPECT_STREQ("TestBody:  Yo Dude\n", buffer);

  dbg_ctx_destroy(dc);
}
#endif
