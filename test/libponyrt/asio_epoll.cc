#include <platform.h>

#include <gtest/gtest.h>

#include <stdint.h>

#if defined(PLATFORM_IS_LINUX)
#include <sys/eventfd.h>
#include <unistd.h>

extern "C" {
  bool ponyint_asio_epoll_read_signal_count(int fd, uint32_t* out_count);
}

static void close_eventfd(int fd)
{
  int rc = close(fd);
  ASSERT_EQ(0, rc);
}

TEST(AsioEpoll, ReadSignalCountReturnsFalseForUnwrittenNonblockingEventfd)
{
  int fd = eventfd(0, EFD_NONBLOCK);
  ASSERT_NE(-1, fd);

  uint32_t count = 123;
  ASSERT_FALSE(ponyint_asio_epoll_read_signal_count(fd, &count));
  ASSERT_EQ((uint32_t)123, count);

  close_eventfd(fd);
}

TEST(AsioEpoll, ReadSignalCountReturnsWrittenValue)
{
  int fd = eventfd(0, EFD_NONBLOCK);
  ASSERT_NE(-1, fd);

  ASSERT_EQ(0, eventfd_write(fd, 7));

  uint32_t count = 0;
  ASSERT_TRUE(ponyint_asio_epoll_read_signal_count(fd, &count));
  ASSERT_EQ((uint32_t)7, count);

  close_eventfd(fd);
}

TEST(AsioEpoll, ReadSignalCountAccumulatesWritesAndDrainsEventfd)
{
  int fd = eventfd(0, EFD_NONBLOCK);
  ASSERT_NE(-1, fd);

  ASSERT_EQ(0, eventfd_write(fd, 3));
  ASSERT_EQ(0, eventfd_write(fd, 4));

  uint32_t count = 0;
  ASSERT_TRUE(ponyint_asio_epoll_read_signal_count(fd, &count));
  ASSERT_EQ((uint32_t)7, count);

  count = 123;
  ASSERT_FALSE(ponyint_asio_epoll_read_signal_count(fd, &count));
  ASSERT_EQ((uint32_t)123, count);

  close_eventfd(fd);
}
#endif
