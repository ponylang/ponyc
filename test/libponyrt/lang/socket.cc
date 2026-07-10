#include <platform.h>
#include <gtest/gtest.h>

#ifndef PLATFORM_IS_WINDOWS

#include <asio/event.h>
#include <lang/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <vector>

static const int SOCKET_TIMEOUT_SECS = 5;
static const int MAX_ELAPSED_MS = 2000;

static_assert((SOCKET_TIMEOUT_SECS * 1000) > MAX_ELAPSED_MS,
  "the socket timeout has to be longer than the elapsed-time bound, or a call "
  "that blocks finishes inside the bound and the tests pass without the flag");

class SocketTest : public testing::Test
{
protected:
  void SetUp() override
  {
    // Puts SIGPIPE on SIG_IGN for the rest of the process, the disposition
    // every Pony program runs with. A send to a closed peer has to come back as
    // an error rather than kill the test binary.
    ASSERT_TRUE(ponyint_os_sockets_init());

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

  // A blocking stream socket with timeouts, and its peer. A non-zero bufsize
  // shrinks the socket buffers so that a test can fill or overrun them; zero
  // leaves the kernel's defaults.
  void make_blocking_stream_pair(int bufsize = 0)
  {
    ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, _stream));

    if(bufsize > 0)
    {
      ASSERT_EQ(0, setsockopt(_stream[0], SOL_SOCKET, SO_SNDBUF, &bufsize,
        sizeof(bufsize)));
      ASSERT_EQ(0, setsockopt(_stream[1], SOL_SOCKET, SO_RCVBUF, &bufsize,
        sizeof(bufsize)));
    }

    ASSERT_NO_FATAL_FAILURE(set_timeouts(_stream[0]));
    ASSERT_FALSE(is_nonblocking(_stream[0]));
    _ev.fd = _stream[0];
  }

  // A blocking datagram socket with timeouts, bound to a loopback port. Its own
  // address goes in _dgram_addr so a test can send to it.
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

    ASSERT_NO_FATAL_FAILURE(set_timeouts(_dgram));
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
    ASSERT_EQ(1, poll(&pfd, 1, SOCKET_TIMEOUT_SECS * 1000));
  }

  static void set_timeouts(int fd)
  {
    struct timeval tv;
    tv.tv_sec = SOCKET_TIMEOUT_SECS;
    tv.tv_usec = 0;
    ASSERT_EQ(0, setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)));
    ASSERT_EQ(0, setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)));
  }

  // Guards the tests against passing for the wrong reason: if the fd were
  // non-blocking, the calls would return without waiting even with the flag
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

class SocketRecvTest : public SocketTest {};

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

// Small enough that filling the send buffer, or overrunning it with one write,
// takes a handful of kilobytes rather than megabytes. Kernels round it, so the
// tests that care about the real capacity read it back with getsockopt.
static const int SMALL_BUFSIZE = 8192;

class SocketSendTest : public SocketTest
{
protected:
  // A macOS send waits for buffer space whatever flags it is given. Its reads
  // honor MSG_DONTWAIT, so only the send tests have to step around this.
  static bool sends_honor_dontwait()
  {
#ifdef PLATFORM_IS_MACOSX
    return false;
#else
    return true;
#endif
  }

  // Put the stream pair's own descriptor in non-blocking mode, the mode every
  // socket the runtime creates is in.
  void make_stream_pair_nonblocking()
  {
    int flags = fcntl(_stream[0], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(_stream[0], F_SETFL, flags | O_NONBLOCK));
  }

  // Leave the stream pair's send buffer with no room in it. The fd goes
  // non-blocking for the fill so that a full buffer announces itself with
  // EAGAIN, and blocking again afterwards, which is the state under test.
  void fill_send_buffer()
  {
    int flags = fcntl(_stream[0], F_GETFL, 0);
    ASSERT_NE(-1, flags);
    ASSERT_NE(-1, fcntl(_stream[0], F_SETFL, flags | O_NONBLOCK));

    std::vector<char> chunk(SMALL_BUFSIZE, 'x');
    bool full = false;

    for(int i = 0; !full && (i < 1024); i++)
    {
      ssize_t w = write(_stream[0], chunk.data(), chunk.size());

      if(w < 0)
      {
        ASSERT_TRUE((errno == EWOULDBLOCK) || (errno == EAGAIN));
        full = true;
      }
    }

    ASSERT_TRUE(full);
    ASSERT_NE(-1, fcntl(_stream[0], F_SETFL, flags));
    ASSERT_FALSE(is_nonblocking(_stream[0]));
  }

  // What the kernel actually gave us, which is not what we asked for: the two
  // buffers together bound how much one write can put into an empty socket.
  size_t effective_capacity()
  {
    int sndbuf = 0;
    int rcvbuf = 0;
    socklen_t len = sizeof(sndbuf);
    EXPECT_EQ(0, getsockopt(_stream[0], SOL_SOCKET, SO_SNDBUF, &sndbuf, &len));
    len = sizeof(rcvbuf);
    EXPECT_EQ(0, getsockopt(_stream[1], SOL_SOCKET, SO_RCVBUF, &rcvbuf, &len));
    return (size_t)sndbuf + (size_t)rcvbuf;
  }

  // An address pointing at the datagram socket this test made.
  void datagram_address(ipaddress_t* ipaddr)
  {
    memset(ipaddr, 0, sizeof(ipaddress_t));
    memcpy(&ipaddr->addr, &_dgram_addr, sizeof(struct sockaddr_in));
  }

  // An address whose family the runtime recognizes. pony_os_sendto rejects one
  // it doesn't before it ever looks at the descriptor, so a test that wants to
  // reach the descriptor has to hand it a real address.
  static void loopback_address(ipaddress_t* ipaddr)
  {
    memset(ipaddr, 0, sizeof(ipaddress_t));
    struct sockaddr_in* addr = (struct sockaddr_in*)&ipaddr->addr;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr->sin_port = htons(9);
  }
};

TEST_F(SocketSendTest, SendvOnFullBlockingSocketDoesNotWait)
{
  if(!sends_honor_dontwait())
    GTEST_SKIP() << "a send ignores MSG_DONTWAIT on this platform";

  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair(SMALL_BUFSIZE));
  ASSERT_NO_FATAL_FAILURE(fill_send_buffer());

  char data[] = "hello";
  struct iovec iov[1];
  iov[0].iov_base = data;
  iov[0].iov_len = 5;
  size_t count = 1;

  auto start = std::chrono::steady_clock::now();
  pony_socket_result_t r = pony_os_sendv(&_ev, iov, 1, &count);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start).count();

  ASSERT_EQ(PONY_SOCKET_RETRY, r);
  ASSERT_EQ((size_t)0, count);
  ASSERT_LT(elapsed, MAX_ELAPSED_MS);
}

TEST_F(SocketSendTest, SendOnFullBlockingSocketDoesNotWait)
{
  if(!sends_honor_dontwait())
    GTEST_SKIP() << "a send ignores MSG_DONTWAIT on this platform";

  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair(SMALL_BUFSIZE));
  ASSERT_NO_FATAL_FAILURE(fill_send_buffer());

  size_t count = 1;

  auto start = std::chrono::steady_clock::now();
  pony_socket_result_t r = pony_os_send(&_ev, "hello", 5, &count);
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start).count();

  ASSERT_EQ(PONY_SOCKET_RETRY, r);
  ASSERT_EQ((size_t)0, count);
  ASSERT_LT(elapsed, MAX_ELAPSED_MS);
}

TEST_F(SocketSendTest, SendvOnBlockingSocketWritesAllBuffers)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  char first[] = "hello";
  char second[] = ", world";
  struct iovec iov[2];
  iov[0].iov_base = first;
  iov[0].iov_len = 5;
  iov[1].iov_base = second;
  iov[1].iov_len = 7;
  size_t count = 0;

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_sendv(&_ev, iov, 2, &count));
  ASSERT_EQ((size_t)12, count);

  char buf[64];
  ASSERT_NO_FATAL_FAILURE(wait_readable(_stream[1]));
  ASSERT_EQ((ssize_t)12, read(_stream[1], buf, sizeof(buf)));
  ASSERT_EQ(0, memcmp(buf, "hello, world", 12));
}

/** TCPConnection._manage_pending_buffer advances its iovecs by the count an OK
 * carries, so a partial write must stay an OK with a short count rather than
 * become a retry.
 */
TEST_F(SocketSendTest, SendvWithOversizedPayloadWritesPartially)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair(SMALL_BUFSIZE));
  ASSERT_NO_FATAL_FAILURE(make_stream_pair_nonblocking());

  // Past what the socket can hold, however the kernel rounded the request.
  size_t total = effective_capacity() * 4;
  std::vector<char> data(total, 'x');
  struct iovec iov[1];
  iov[0].iov_base = data.data();
  iov[0].iov_len = total;
  size_t count = 0;

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_sendv(&_ev, iov, 1, &count));
  ASSERT_LT((size_t)0, count);
  ASSERT_LT(count, total);
}

TEST_F(SocketSendTest, SendWithOversizedPayloadWritesPartially)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair(SMALL_BUFSIZE));
  ASSERT_NO_FATAL_FAILURE(make_stream_pair_nonblocking());

  size_t total = effective_capacity() * 4;
  std::vector<char> data(total, 'x');
  size_t count = 0;

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_send(&_ev, data.data(), total, &count));
  ASSERT_LT((size_t)0, count);
  ASSERT_LT(count, total);
}

TEST_F(SocketSendTest, SendOnBlockingSocketWritesData)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  size_t count = 0;
  ASSERT_EQ(PONY_SOCKET_OK, pony_os_send(&_ev, "hello", 5, &count));
  ASSERT_EQ((size_t)5, count);

  char buf[64];
  ASSERT_NO_FATAL_FAILURE(wait_readable(_stream[1]));
  ASSERT_EQ((ssize_t)5, read(_stream[1], buf, sizeof(buf)));
  ASSERT_EQ(0, memcmp(buf, "hello", 5));
}

/** A closed peer must not report RETRY: the stdlib write loop would answer a
 * retry by writing to the dead socket again.
 */
TEST_F(SocketSendTest, SendvOnPeerClosedSocketReturnsError)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  ASSERT_EQ(0, close(_stream[1]));
  _stream[1] = -1;

  char data[] = "hello";
  struct iovec iov[1];
  iov[0].iov_base = data;
  iov[0].iov_len = 5;
  size_t count = 1;

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_sendv(&_ev, iov, 1, &count));
  ASSERT_EQ((size_t)0, count);
}

TEST_F(SocketSendTest, SendOnPeerClosedSocketReturnsError)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  ASSERT_EQ(0, close(_stream[1]));
  _stream[1] = -1;

  size_t count = 1;

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_send(&_ev, "hello", 5, &count));
  ASSERT_EQ((size_t)0, count);
}

/** Only EAGAIN becomes a retry. Were ENOTSOCK to become one, the stdlib write
 * loop would retry a dead descriptor forever.
 */
TEST_F(SocketSendTest, SendvOnNonSocketFdReturnsError)
{
  ASSERT_EQ(0, pipe(_pipe));
  _ev.fd = _pipe[1];

  char data[] = "hello";
  struct iovec iov[1];
  iov[0].iov_base = data;
  iov[0].iov_len = 5;
  size_t count = 1;

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_sendv(&_ev, iov, 1, &count));
  ASSERT_EQ((size_t)0, count);
}

TEST_F(SocketSendTest, SendOnNonSocketFdReturnsError)
{
  ASSERT_EQ(0, pipe(_pipe));
  _ev.fd = _pipe[1];

  size_t count = 1;

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_send(&_ev, "hello", 5, &count));
  ASSERT_EQ((size_t)0, count);
}

TEST_F(SocketSendTest, SendToOnBlockingSocketSendsDatagram)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_datagram_socket());

  ipaddress_t ipaddr;
  datagram_address(&ipaddr);
  size_t count = 0;

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_sendto(_dgram, "hello", 5, &ipaddr,
    &count));
  ASSERT_EQ((size_t)5, count);

  // The socket sent to itself, so the datagram comes back to it.
  char buf[64];
  ASSERT_NO_FATAL_FAILURE(wait_readable(_dgram));
  ASSERT_EQ((ssize_t)5, recv(_dgram, buf, sizeof(buf), 0));
  ASSERT_EQ(0, memcmp(buf, "hello", 5));
}

TEST_F(SocketSendTest, SendToOnNonSocketFdReturnsError)
{
  ASSERT_EQ(0, pipe(_pipe));

  ipaddress_t ipaddr;
  loopback_address(&ipaddr);
  size_t count = 1;

  ASSERT_EQ(PONY_SOCKET_ERROR, pony_os_sendto(_pipe[1], "hello", 5, &ipaddr,
    &count));
  ASSERT_EQ((size_t)0, count);
}

TEST_F(SocketSendTest, WritevOnBlockingSocketWritesAllBuffers)
{
  ASSERT_NO_FATAL_FAILURE(make_blocking_stream_pair());

  char first[] = "hello";
  char second[] = ", world";
  struct iovec iov[2];
  iov[0].iov_base = first;
  iov[0].iov_len = 5;
  iov[1].iov_base = second;
  iov[1].iov_len = 7;
  size_t count = 0;

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_writev(&_ev, iov, 2, &count));
  ASSERT_EQ((size_t)12, count);

  char buf[64];
  ASSERT_NO_FATAL_FAILURE(wait_readable(_stream[1]));
  ASSERT_EQ((ssize_t)12, read(_stream[1], buf, sizeof(buf)));
  ASSERT_EQ(0, memcmp(buf, "hello, world", 12));
}

/** pony_os_writev takes any descriptor and pony_os_sendv takes a socket, which
 * is why the runtime carries both. A sendmsg behind this name would return
 * ERROR here.
 */
TEST_F(SocketSendTest, WritevOnNonSocketFdWritesData)
{
  ASSERT_EQ(0, pipe(_pipe));
  _ev.fd = _pipe[1];

  char first[] = "hello";
  char second[] = ", world";
  struct iovec iov[2];
  iov[0].iov_base = first;
  iov[0].iov_len = 5;
  iov[1].iov_base = second;
  iov[1].iov_len = 7;
  size_t count = 0;

  ASSERT_EQ(PONY_SOCKET_OK, pony_os_writev(&_ev, iov, 2, &count));
  ASSERT_EQ((size_t)12, count);

  char buf[64];
  ASSERT_EQ((ssize_t)12, read(_pipe[0], buf, sizeof(buf)));
  ASSERT_EQ(0, memcmp(buf, "hello, world", 12));
}

#endif
