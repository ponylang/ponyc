#include <platform.h>
#include <gtest/gtest.h>

#ifndef PLATFORM_IS_WINDOWS

#include <asio/event.h>
#include <lang/socket.h>

#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>

/** pony_os_recv and pony_os_recvfrom pass MSG_DONTWAIT so that a read never
 * blocks, whatever mode the fd is in. The runtime only ever creates
 * non-blocking sockets, so these tests hand it a blocking one -- the state a
 * reused fd number can put it in -- and check that the call still returns
 * without waiting for data.
 *
 * SO_RCVTIMEO bounds the failure: were MSG_DONTWAIT dropped, the recv would
 * block until that timeout rather than forever, so the test fails on the
 * elapsed-time assertion instead of hanging the suite. The result assertions
 * pass either way; the elapsed time is what tells the two apart.
 */

static const int RECV_TIMEOUT_SECS = 5;
static const int MAX_ELAPSED_MS = 2000;

static_assert((RECV_TIMEOUT_SECS * 1000) > MAX_ELAPSED_MS,
  "the receive timeout has to be longer than the elapsed-time bound, or a read "
  "that blocks finishes inside the bound and the tests pass without the flag");

class SocketRecvTest : public testing::Test
{
protected:
  void SetUp() override
  {
    memset(&_ev, 0, sizeof(_ev));
    memset(&_dgram_addr, 0, sizeof(_dgram_addr));
    _stream[0] = -1;
    _stream[1] = -1;
    _dgram = -1;
    _pipe[0] = -1;
    _pipe[1] = -1;
  }

  void TearDown() override
  {
    if(_stream[0] >= 0)
      close(_stream[0]);

    if(_stream[1] >= 0)
      close(_stream[1]);

    if(_dgram >= 0)
      close(_dgram);

    if(_pipe[0] >= 0)
      close(_pipe[0]);

    if(_pipe[1] >= 0)
      close(_pipe[1]);
  }

  // A blocking stream socket with a receive timeout, and its peer.
  void make_blocking_stream_pair()
  {
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, _stream));
    ASSERT_NO_FATAL_FAILURE(set_recv_timeout(_stream[0]));
    ASSERT_FALSE(is_nonblocking(_stream[0]));
    _ev.fd = _stream[0];
  }

  // A blocking datagram socket with a receive timeout, bound to a loopback
  // port. Its own address goes in _dgram_addr so a test can send to it.
  void make_blocking_datagram_socket()
  {
    _dgram = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_LE(0, _dgram);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ASSERT_EQ(0, bind(_dgram, (struct sockaddr*)&addr, sizeof(addr)));

    socklen_t addrlen = sizeof(_dgram_addr);
    ASSERT_EQ(0, getsockname(_dgram, (struct sockaddr*)&_dgram_addr, &addrlen));

    ASSERT_NO_FATAL_FAILURE(set_recv_timeout(_dgram));
    ASSERT_FALSE(is_nonblocking(_dgram));
    _ev.fd = _dgram;
  }

  // Wait for the fd to have data, so that a non-blocking read can't outrun the
  // kernel's loopback delivery. This is also the state the asio backend has a
  // socket in when it calls these functions.
  static void wait_readable(int fd)
  {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    ASSERT_EQ(1, poll(&pfd, 1, RECV_TIMEOUT_SECS * 1000));
  }

  static void set_recv_timeout(int fd)
  {
    struct timeval tv;
    tv.tv_sec = RECV_TIMEOUT_SECS;
    tv.tv_usec = 0;
    ASSERT_EQ(0, setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));
  }

  // Guards the tests against passing for the wrong reason: if the fd were
  // non-blocking, the reads would return without waiting even with the flag
  // removed.
  static bool is_nonblocking(int fd)
  {
    return (fcntl(fd, F_GETFL, 0) & O_NONBLOCK) != 0;
  }

  asio_event_t _ev;
  int _stream[2];
  int _dgram;
  int _pipe[2];
  struct sockaddr_storage _dgram_addr;
};

/** A read on a blocking stream socket with nothing to read returns RETRY
 * without waiting for data.
 */
TEST_F(SocketRecvTest, RecvOnEmptyBlockingSocketDoesNotWait)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  char buf[64];
  size_t count = 1;

  auto start = std::chrono::steady_clock::now();
  pony_socket_result_t r = pony_os_recv(&_ev, buf, sizeof(buf), &count);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start).count();

  ASSERT_EQ(PONY_SOCKET_RETRY, r);
  ASSERT_EQ((size_t)0, count);
  ASSERT_LT(elapsed, MAX_ELAPSED_MS);
}

/** A read on a blocking datagram socket with nothing to read returns RETRY
 * without waiting for a datagram.
 */
TEST_F(SocketRecvTest, RecvFromOnEmptyBlockingSocketDoesNotWait)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_datagram_socket());

  char buf[64];
  size_t count = 1;
  ipaddress_t ipaddr;
  memset(&ipaddr, 0, sizeof(ipaddr));

  auto start = std::chrono::steady_clock::now();
  pony_socket_result_t r = pony_os_recvfrom(&_ev, buf, sizeof(buf), &ipaddr,
    &count);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start).count();

  ASSERT_EQ(PONY_SOCKET_RETRY, r);
  ASSERT_EQ((size_t)0, count);
  ASSERT_LT(elapsed, MAX_ELAPSED_MS);
}

/** MSG_DONTWAIT doesn't stop a blocking stream socket from delivering data
 * that is already there.
 */
TEST_F(SocketRecvTest, RecvOnBlockingSocketReadsAvailableData)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  ASSERT_EQ((ssize_t)5, write(_stream[1], "hello", 5));
  ASSERT_NO_FATAL_FAILURE(wait_readable(_stream[0]));

  char buf[64];
  size_t count = 0;

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_recv(&_ev, buf, sizeof(buf), &count));
  ASSERT_EQ((size_t)5, count);
  ASSERT_EQ(0, memcmp(buf, "hello", 5));
}

/** MSG_DONTWAIT doesn't stop a blocking datagram socket from delivering a
 * datagram that is already there, or from reporting the sender's address.
 */
TEST_F(SocketRecvTest, RecvFromOnBlockingSocketReadsAvailableDatagram)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_datagram_socket());

  ASSERT_EQ((ssize_t)5, sendto(_dgram, "hello", 5, 0,
    (struct sockaddr*)&_dgram_addr, sizeof(struct sockaddr_in)));
  ASSERT_NO_FATAL_FAILURE(wait_readable(_dgram));

  char buf[64];
  size_t count = 0;
  ipaddress_t ipaddr;
  memset(&ipaddr, 0, sizeof(ipaddr));

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_recvfrom(&_ev, buf, sizeof(buf), &ipaddr,
    &count));
  ASSERT_EQ((size_t)5, count);
  ASSERT_EQ(0, memcmp(buf, "hello", 5));

  // The socket sent to itself, so the sender's address is its own.
  const struct sockaddr_in* from = (const struct sockaddr_in*)&ipaddr.addr;
  const struct sockaddr_in* self = (const struct sockaddr_in*)&_dgram_addr;
  ASSERT_EQ(AF_INET, ipaddr.addr.ss_family);
  ASSERT_EQ(self->sin_port, from->sin_port);
  ASSERT_EQ(self->sin_addr.s_addr, from->sin_addr.s_addr);
}

/** An empty datagram is a valid read, so recvfrom reports a zero-byte OK.
 * MSG_DONTWAIT does not turn it into a retry, which the caller could not tell
 * apart from an empty socket.
 */
TEST_F(SocketRecvTest, RecvFromOnEmptyDatagramReturnsOk)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_datagram_socket());

  ASSERT_EQ((ssize_t)0, sendto(_dgram, "", 0, 0,
    (struct sockaddr*)&_dgram_addr, sizeof(struct sockaddr_in)));
  ASSERT_NO_FATAL_FAILURE(wait_readable(_dgram));

  char buf[64];
  size_t count = 1;
  ipaddress_t ipaddr;
  memset(&ipaddr, 0, sizeof(ipaddr));

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_recvfrom(&_ev, buf, sizeof(buf), &ipaddr,
    &count));
  ASSERT_EQ((size_t)0, count);
  ASSERT_EQ(AF_INET, ipaddr.addr.ss_family);
}

/** A read on a stream socket whose peer has closed reports ERROR, not a
 * zero-byte OK. MSG_DONTWAIT does not turn the peer's close into a retry.
 */
TEST_F(SocketRecvTest, RecvOnPeerClosedSocketReturnsError)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  ASSERT_EQ(0, close(_stream[1]));
  _stream[1] = -1;
  ASSERT_NO_FATAL_FAILURE(wait_readable(_stream[0]));

  char buf[64];
  size_t count = 1;

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_recv(&_ev, buf, sizeof(buf), &count));
  ASSERT_EQ((size_t)0, count);
}

/** A read on a descriptor that isn't a socket reports ERROR. Only EAGAIN
 * becomes a retry; were any other error to become one, the stdlib read loop
 * would retry a dead descriptor forever.
 */
TEST_F(SocketRecvTest, RecvOnNonSocketFdReturnsError)
{
  ASSERT_EQ(0, pipe(_pipe));
  _ev.fd = _pipe[0];

  char buf[64];
  size_t count = 1;

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_recv(&_ev, buf, sizeof(buf), &count));
  ASSERT_EQ((size_t)0, count);
}

/** As for pony_os_recv: a descriptor that isn't a socket is an error, not a
 * retry.
 */
TEST_F(SocketRecvTest, RecvFromOnNonSocketFdReturnsError)
{
  ASSERT_EQ(0, pipe(_pipe));
  _ev.fd = _pipe[0];

  char buf[64];
  size_t count = 1;
  ipaddress_t ipaddr;
  memset(&ipaddr, 0, sizeof(ipaddr));

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_recvfrom(&_ev, buf, sizeof(buf), &ipaddr,
    &count));
  ASSERT_EQ((size_t)0, count);
}

#endif
