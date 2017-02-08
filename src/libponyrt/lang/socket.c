#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#include "lang.h"
#include "../asio/asio.h"
#include "../asio/event.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifdef PLATFORM_IS_WINDOWS
// Disable warnings about deprecated non-unicode WSA functions.
#pragma warning(disable:4996)

#include "../mem/pool.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#include <mswsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
typedef int SOCKET;
#endif

#ifdef PLATFORM_IS_POSIX_BASED
#include <signal.h>
#endif

PONY_EXTERN_C_BEGIN

void pony_os_socket_close(int fd);

// This must match the pony NetAddress type in packages/net.
typedef struct
{
  pony_type_t* type;
  struct sockaddr_storage addr;
} ipaddress_t;

static socklen_t address_length(ipaddress_t* ipaddr)
{
  switch(ipaddr->addr.ss_family)
  {
    case AF_INET:
      return sizeof(struct sockaddr_in);

    case AF_INET6:
      return sizeof(struct sockaddr_in6);
  }

  return (socklen_t)-1;
}

// Transform "any" addresses into loopback addresses.
static bool map_any_to_loopback(struct sockaddr* addr)
{
  switch(addr->sa_family)
  {
    case AF_INET:
    {
      struct sockaddr_in* in = (struct sockaddr_in*)addr;

      if(in->sin_addr.s_addr == INADDR_ANY)
      {
        in->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        return true;
      }

      break;
    }

    case AF_INET6:
    {
      struct sockaddr_in6* in = (struct sockaddr_in6*)addr;

      if(memcmp(&in->sin6_addr, &in6addr_any, sizeof(struct in6_addr)) == 0)
      {
        memcpy(&in->sin6_addr, &in6addr_loopback, sizeof(struct in6_addr));
        return true;
      }
    }

    default: {}
  }

  return false;
}

static struct addrinfo* os_addrinfo_intern(int family, int socktype,
  int proto, const char* host, const char* service, bool passive)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = family;
  hints.ai_socktype = socktype;
  hints.ai_protocol = proto;

  if(passive)
    hints.ai_flags |= AI_PASSIVE;

  if((host != NULL) && (host[0] == '\0'))
    host = NULL;

  struct addrinfo *result;

  if(getaddrinfo(host, service, &hints, &result) != 0)
    return NULL;

  return result;
}

#if defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_FREEBSD)
static int set_nonblocking(int s)
{
  int flags = fcntl(s, F_GETFL, 0);
  return fcntl(s, F_SETFL, flags | O_NONBLOCK);
}
#endif

#ifdef PLATFORM_IS_WINDOWS

#define IOCP_ACCEPT_ADDR_LEN (sizeof(struct sockaddr_storage) + 16)

static LPFN_CONNECTEX g_ConnectEx;
static LPFN_ACCEPTEX g_AcceptEx;

typedef enum
{
  IOCP_CONNECT,
  IOCP_ACCEPT,
  IOCP_SEND,
  IOCP_RECV,
  IOCP_NOP
} iocp_op_t;

typedef struct iocp_t
{
  OVERLAPPED ov;
  iocp_op_t op;
  int from_len;
  asio_event_t* ev;
} iocp_t;

typedef struct iocp_accept_t
{
  iocp_t iocp;
  SOCKET ns;
  char buf[IOCP_ACCEPT_ADDR_LEN * 2];
} iocp_accept_t;

static iocp_t* iocp_create(iocp_op_t op, asio_event_t* ev)
{
  iocp_t* iocp = POOL_ALLOC(iocp_t);
  memset(&iocp->ov, 0, sizeof(OVERLAPPED));
  iocp->op = op;
  iocp->ev = ev;

  return iocp;
}

static void iocp_destroy(iocp_t* iocp)
{
  POOL_FREE(iocp_t, iocp);
}

static iocp_accept_t* iocp_accept_create(SOCKET s, asio_event_t* ev)
{
  iocp_accept_t* iocp = POOL_ALLOC(iocp_accept_t);
  memset(&iocp->iocp.ov, 0, sizeof(OVERLAPPED));
  iocp->iocp.op = IOCP_ACCEPT;
  iocp->iocp.ev = ev;
  iocp->ns = s;

  return iocp;
}

static void iocp_accept_destroy(iocp_accept_t* iocp)
{
  POOL_FREE(iocp_accept_t, iocp);
}

static void CALLBACK iocp_callback(DWORD err, DWORD bytes, OVERLAPPED* ov)
{
  iocp_t* iocp = (iocp_t*)ov;

  switch(iocp->op)
  {
    case IOCP_CONNECT:
    {
      if(err == ERROR_SUCCESS)
      {
        // Update the connect context.
        setsockopt((SOCKET)iocp->ev->fd, SOL_SOCKET,
          SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
      }

      // Dispatch a write event.
      pony_asio_event_send(iocp->ev, ASIO_WRITE, 0);
      iocp_destroy(iocp);
      break;
    }

    case IOCP_ACCEPT:
    {
      iocp_accept_t* acc = (iocp_accept_t*)iocp;

      if(err == ERROR_SUCCESS)
      {
        // Update the accept context.
        SOCKET s = (SOCKET)iocp->ev->fd;

        setsockopt(acc->ns, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
          (char*)&s, sizeof(SOCKET));
      } else {
        // Close the new socket.
        closesocket(acc->ns);
        acc->ns = INVALID_SOCKET;
      }

      // Dispatch a read event with the new socket as the argument.
      pony_asio_event_send(iocp->ev, ASIO_READ, (int)acc->ns);
      iocp_accept_destroy(acc);
      break;
    }

    case IOCP_SEND:
    {
      if(err == ERROR_SUCCESS)
      {
        // Dispatch a write event with the number of bytes written.
        pony_asio_event_send(iocp->ev, ASIO_WRITE, bytes);
      } else {
        // Dispatch a write event with zero bytes to indicate a close.
        pony_asio_event_send(iocp->ev, ASIO_WRITE, 0);
      }

      iocp_destroy(iocp);
      break;
    }

    case IOCP_RECV:
    {
      if(err == ERROR_SUCCESS)
      {
        // Dispatch a read event with the number of bytes read.
        pony_asio_event_send(iocp->ev, ASIO_READ, bytes);
      } else {
        // Dispatch a read event with zero bytes to indicate a close.
        pony_asio_event_send(iocp->ev, ASIO_READ, 0);
      }

      iocp_destroy(iocp);
      break;
    }

    case IOCP_NOP:
      // Don't care, do nothing
      iocp_destroy(iocp);
      break;
  }
}

static bool iocp_connect(asio_event_t* ev, struct addrinfo *p)
{
  SOCKET s = (SOCKET)ev->fd;
  iocp_t* iocp = iocp_create(IOCP_CONNECT, ev);

  if(!g_ConnectEx(s, p->ai_addr, (int)p->ai_addrlen, NULL, 0, NULL, &iocp->ov))
  {
    if(GetLastError() != ERROR_IO_PENDING)
    {
      iocp_destroy(iocp);
      return false;
    }
  }

  return true;
}

static bool iocp_accept(asio_event_t* ev)
{
  SOCKET s = (SOCKET)ev->fd;
  WSAPROTOCOL_INFO proto;

  if(WSADuplicateSocket(s, GetCurrentProcessId(), &proto) != 0)
    return false;

  SOCKET ns = WSASocket(proto.iAddressFamily, proto.iSocketType,
    proto.iProtocol, NULL, 0, WSA_FLAG_OVERLAPPED);

  if((ns == INVALID_SOCKET) ||
    !BindIoCompletionCallback((HANDLE)ns, iocp_callback, 0))
  {
    return false;
  }

  iocp_accept_t* iocp = iocp_accept_create(ns, ev);
  DWORD bytes;

  if(!g_AcceptEx(s, ns, iocp->buf, 0, IOCP_ACCEPT_ADDR_LEN,
    IOCP_ACCEPT_ADDR_LEN, &bytes, &iocp->iocp.ov))
  {
    if(GetLastError() != ERROR_IO_PENDING)
    {
      iocp_accept_destroy(iocp);
      return false;
    }
  }

  return true;
}

static bool iocp_send(asio_event_t* ev, const char* data, size_t len)
{
  SOCKET s = (SOCKET)ev->fd;
  iocp_t* iocp = iocp_create(IOCP_SEND, ev);
  DWORD sent;

  WSABUF buf;
  buf.buf = (char*)data;
  buf.len = (u_long)len;

  if(WSASend(s, &buf, 1, &sent, 0, &iocp->ov, NULL) != 0)
  {
    if(GetLastError() != WSA_IO_PENDING)
    {
      iocp_destroy(iocp);
      return false;
    }
  }

  return true;
}

static bool iocp_recv(asio_event_t* ev, char* data, size_t len)
{
  SOCKET s = (SOCKET)ev->fd;
  iocp_t* iocp = iocp_create(IOCP_RECV, ev);
  DWORD received;
  DWORD flags = 0;

  WSABUF buf;
  buf.buf = data;
  buf.len = (u_long)len;

  if(WSARecv(s, &buf, 1, &received, &flags, &iocp->ov, NULL) != 0)
  {
    if(GetLastError() != WSA_IO_PENDING)
    {
      iocp_destroy(iocp);
      return false;
    }
  }

  return true;
}

static bool iocp_sendto(int fd, const char* data, size_t len,
  ipaddress_t* ipaddr)
{
  socklen_t socklen = address_length(ipaddr);

  if(socklen == (socklen_t)-1)
    return false;

  iocp_t* iocp = iocp_create(IOCP_NOP, NULL);

  WSABUF buf;
  buf.buf = (char*)data;
  buf.len = (u_long)len;

  if(WSASendTo((SOCKET)fd, &buf, 1, NULL, 0, (struct sockaddr*)&ipaddr->addr,
    socklen, &iocp->ov, NULL) != 0)
  {
    if(GetLastError() != WSA_IO_PENDING)
    {
      iocp_destroy(iocp);
      return false;
    }
  }

  return true;
}

static bool iocp_recvfrom(asio_event_t* ev, char* data, size_t len,
  ipaddress_t* ipaddr)
{
  SOCKET s = (SOCKET)ev->fd;
  iocp_t* iocp = iocp_create(IOCP_RECV, ev);
  DWORD flags = 0;

  WSABUF buf;
  buf.buf = data;
  buf.len = (u_long)len;

  iocp->from_len = sizeof(ipaddr->addr);

  if(WSARecvFrom(s, &buf, 1, NULL, &flags, (struct sockaddr*)&ipaddr->addr,
    &iocp->from_len, &iocp->ov, NULL) != 0)
  {
    if(GetLastError() != WSA_IO_PENDING)
    {
      iocp_destroy(iocp);
      return false;
    }
  }

  return true;
}

#endif

static int socket_from_addrinfo(struct addrinfo* p, bool reuse)
{
#if defined(PLATFORM_IS_LINUX)
  int fd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK,
    p->ai_protocol);
#elif defined(PLATFORM_IS_WINDOWS)
  UINT_PTR skt = WSASocket(p->ai_family, p->ai_socktype, p->ai_protocol, NULL,
    0, WSA_FLAG_OVERLAPPED);
  assert((skt == INVALID_SOCKET) || ((skt >> 31) == 0));
  int fd = (int)skt;
#else
  int fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#endif

  if(fd < 0)
    return -1;

  int r = 0;

  if(reuse)
  {
    int reuseaddr = 1;
    r |= setsockopt((SOCKET)fd, SOL_SOCKET, SO_REUSEADDR,
      (const char*)&reuseaddr, sizeof(int));
  }

#if defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_FREEBSD)
  r |= set_nonblocking(fd);
#endif

#ifdef PLATFORM_IS_WINDOWS
  if(!BindIoCompletionCallback((HANDLE)(UINT_PTR)fd, iocp_callback, 0))
    r = 1;
#endif

  if(r == 0)
    return fd;

  pony_os_socket_close(fd);
  return -1;
}

static asio_event_t* os_listen(pony_actor_t* owner, int fd,
  struct addrinfo *p, int proto)
{
  if(bind((SOCKET)fd, p->ai_addr, (int)p->ai_addrlen) != 0)
  {
    pony_os_socket_close(fd);
    return NULL;
  }

  if(p->ai_socktype == SOCK_STREAM)
  {
    if(listen((SOCKET)fd, SOMAXCONN) != 0)
    {
      pony_os_socket_close(fd);
      return NULL;
    }
  }

  // Create an event and subscribe it.
  asio_event_t* ev = pony_asio_event_create(owner, fd, ASIO_READ, 0, true);

#ifdef PLATFORM_IS_WINDOWS
  // Start accept for TCP connections, but not for UDP.
  if(proto == IPPROTO_TCP)
  {
    if(!iocp_accept(ev))
    {
      pony_asio_event_unsubscribe(ev);
      pony_os_socket_close(fd);
      return NULL;
    }
  }
#else
  (void)proto;
#endif

  return ev;
}

static bool os_connect(pony_actor_t* owner, int fd, struct addrinfo *p,
  const char* from)
{
  map_any_to_loopback(p->ai_addr);

  bool need_bind = (from != NULL) && (from[0] != '\0');

  if(need_bind)
  {
    struct addrinfo* result = os_addrinfo_intern(p->ai_family, 0, 0, from,
      NULL, false);
    struct addrinfo* lp = result;
    bool bound = false;

    while(lp != NULL)
    {
      if(bind((SOCKET)fd, lp->ai_addr, (int)lp->ai_addrlen) == 0)
      {
        bound = true;
        break;
      }

      lp = lp->ai_next;
    }

    freeaddrinfo(result);

    if(!bound)
    {
      pony_os_socket_close(fd);
      return false;
    }
  }

#ifdef PLATFORM_IS_WINDOWS
  if(!need_bind)
  {
    // ConnectEx requires bind.
    struct sockaddr_storage addr = {0};
    addr.ss_family = p->ai_family;

    if(bind((SOCKET)fd, (struct sockaddr*)&addr, (int)p->ai_addrlen) != 0)
    {
      pony_os_socket_close(fd);
      return false;
    }
  }

  // Create an event and subscribe it.
  asio_event_t* ev = pony_asio_event_create(owner, fd, ASIO_READ | ASIO_WRITE,
    0, true);

  if(!iocp_connect(ev, p))
  {
    pony_asio_event_unsubscribe(ev);
    pony_os_socket_close(fd);
    return false;
  }
#else
  int r = connect(fd, p->ai_addr, (int)p->ai_addrlen);

  if((r != 0) && (errno != EINPROGRESS))
  {
    pony_os_socket_close(fd);
    return false;
  }

#ifndef PLATFORM_IS_WINDOWS
  // Create an event and subscribe it.
  pony_asio_event_create(owner, fd, ASIO_READ | ASIO_WRITE | ASIO_ONESHOT, 0, true);
#else
  // Create an event and subscribe it.
  pony_asio_event_create(owner, fd, ASIO_READ | ASIO_WRITE, 0, true);
#endif
#endif

  return true;
}

/**
 * This finds an address to listen on and returns either an asio_event_t or
 * null.
 */
static asio_event_t* os_socket_listen(pony_actor_t* owner, const char* host,
  const char* service, int family, int socktype, int proto)
{
  struct addrinfo* result = os_addrinfo_intern(family, socktype, proto, host,
    service, true);

  struct addrinfo* p = result;

  while(p != NULL)
  {
    int fd = socket_from_addrinfo(p, true);

    if(fd != -1)
    {
      asio_event_t* ev = os_listen(owner, fd, p, proto);
      freeaddrinfo(result);
      return ev;
    }

    p = p->ai_next;
  }

  freeaddrinfo(result);
  return NULL;
}

/**
 * This starts Happy Eyeballs and returns * the number of connection attempts
 * in-flight, which may be 0.
 */
static int os_socket_connect(pony_actor_t* owner, const char* host,
  const char* service, const char* from, int family, int socktype, int proto)
{
  bool reuse = (from == NULL) || (from[0] != '\0');

  struct addrinfo* result = os_addrinfo_intern(family, socktype, proto, host,
    service, false);

  struct addrinfo* p = result;
  int count = 0;

  while(p != NULL)
  {
    int fd = socket_from_addrinfo(p, reuse);

    if(fd != -1)
    {
      if(os_connect(owner, fd, p, from))
        count++;
    }

    p = p->ai_next;
  }

  freeaddrinfo(result);
  return count;
}

asio_event_t* pony_os_listen_tcp(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_UNSPEC, SOCK_STREAM,
    IPPROTO_TCP);
}

asio_event_t* pony_os_listen_tcp4(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET, SOCK_STREAM,
    IPPROTO_TCP);
}

asio_event_t* pony_os_listen_tcp6(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET6, SOCK_STREAM,
    IPPROTO_TCP);
}

asio_event_t* pony_os_listen_udp(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_UNSPEC, SOCK_DGRAM,
    IPPROTO_UDP);
}

asio_event_t* pony_os_listen_udp4(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET, SOCK_DGRAM,
    IPPROTO_UDP);
}

asio_event_t* pony_os_listen_udp6(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET6, SOCK_DGRAM,
    IPPROTO_UDP);
}

int pony_os_connect_tcp(pony_actor_t* owner, const char* host,
  const char* service, const char* from)
{
  return os_socket_connect(owner, host, service, from, AF_UNSPEC, SOCK_STREAM,
    IPPROTO_TCP);
}

int pony_os_connect_tcp4(pony_actor_t* owner, const char* host,
  const char* service, const char* from)
{
  return os_socket_connect(owner, host, service, from, AF_INET, SOCK_STREAM,
    IPPROTO_TCP);
}

int pony_os_connect_tcp6(pony_actor_t* owner, const char* host,
  const char* service, const char* from)
{
  return os_socket_connect(owner, host, service, from, AF_INET6, SOCK_STREAM,
    IPPROTO_TCP);
}

int pony_os_accept(asio_event_t* ev)
{
#if defined(PLATFORM_IS_WINDOWS)
  // Queue an IOCP accept and return an INVALID_SOCKET.
  SOCKET ns = INVALID_SOCKET;
  iocp_accept(ev);
#elif defined(PLATFORM_IS_LINUX)
  int ns = accept4(ev->fd, NULL, NULL, SOCK_NONBLOCK);

  if(ns == -1 && (errno == EWOULDBLOCK || errno == EAGAIN))
    ns = 0;
#else
  int ns = accept(ev->fd, NULL, NULL);

  if(ns != -1)
    set_nonblocking(ns);
  else if(errno == EWOULDBLOCK || errno == EAGAIN)
    ns = 0;
#endif

  return (int)ns;
}

// Check this when a connection gets its first writeable event.
bool pony_os_connected(int fd)
{
  int val = 0;
  socklen_t len = sizeof(int);

  if(getsockopt((SOCKET)fd, SOL_SOCKET, SO_ERROR, (char*)&val, &len) == -1)
    return false;

  return val == 0;
}

static int address_family(int length)
{
  switch(length)
  {
    case 4:
      return AF_INET;

    case 16:
      return AF_INET6;
  }

  return -1;
}

bool pony_os_nameinfo(ipaddress_t* ipaddr, char** rhost, char** rserv,
  bool reversedns, bool servicename)
{
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];

  socklen_t len = address_length(ipaddr);

  if(len == (socklen_t)-1)
    return false;

  int flags = 0;

  if(!reversedns)
    flags |= NI_NUMERICHOST;

  if(!servicename)
    flags |= NI_NUMERICSERV;

  int r = getnameinfo((struct sockaddr*)&ipaddr->addr, len, host, NI_MAXHOST,
    serv, NI_MAXSERV, flags);

  if(r != 0)
    return false;

  pony_ctx_t* ctx = pony_ctx();

  size_t hostlen = strlen(host);
  *rhost = (char*)pony_alloc(ctx, hostlen + 1);
  memcpy(*rhost, host, hostlen + 1);

  size_t servlen = strlen(serv);
  *rserv = (char*)pony_alloc(ctx, servlen + 1);
  memcpy(*rserv, serv, servlen + 1);

  return true;
}

struct addrinfo* pony_os_addrinfo(int family, const char* host,
  const char* service)
{
  switch(family)
  {
    case 0: family = AF_UNSPEC; break;
    case 1: family = AF_INET; break;
    case 2: family = AF_INET6; break;
    default: return NULL;
  }

  return os_addrinfo_intern(family, 0, 0, host, service, true);
}

void pony_os_getaddr(struct addrinfo* addr, ipaddress_t* ipaddr)
{
  memcpy(&ipaddr->addr, addr->ai_addr, addr->ai_addrlen);
  map_any_to_loopback((struct sockaddr*)&ipaddr->addr);
}

struct addrinfo* pony_os_nextaddr(struct addrinfo* addr)
{
  return addr->ai_next;
}

char* pony_os_ip_string(void* src, int len)
{
  char dst[INET6_ADDRSTRLEN];
  int family = address_family(len);

  if(family == -1)
    return NULL;

  if(inet_ntop(family, src, dst, INET6_ADDRSTRLEN))
    return NULL;

  size_t dstlen = strlen(dst);
  char* result = (char*)pony_alloc(pony_ctx(), dstlen + 1);
  memcpy(result, dst, dstlen + 1);

  return result;
}

bool pony_os_ipv4(ipaddress_t* ipaddr)
{
  return ipaddr->addr.ss_family == AF_INET;
}

bool pony_os_ipv6(ipaddress_t* ipaddr)
{
  return ipaddr->addr.ss_family == AF_INET6;
}

bool pony_os_sockname(int fd, ipaddress_t* ipaddr)
{
  socklen_t len = sizeof(struct sockaddr_storage);

  if(getsockname((SOCKET)fd, (struct sockaddr*)&ipaddr->addr, &len) != 0)
    return false;

  map_any_to_loopback((struct sockaddr*)&ipaddr->addr);
  return true;
}

bool pony_os_peername(int fd, ipaddress_t* ipaddr)
{
  socklen_t len = sizeof(struct sockaddr_storage);

  if(getpeername((SOCKET)fd, (struct sockaddr*)&ipaddr->addr, &len) != 0)
    return false;

  map_any_to_loopback((struct sockaddr*)&ipaddr->addr);
  return true;
}

bool pony_os_host_ip4(const char* host)
{
  struct in_addr addr;
  return inet_pton(AF_INET, host, &addr) == 1;
}

bool pony_os_host_ip6(const char* host)
{
  struct in6_addr addr;
  return inet_pton(AF_INET6, host, &addr) == 1;
}

size_t pony_os_send(asio_event_t* ev, const char* buf, size_t len)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_send(ev, buf, len))
    pony_throw();

  return 0;
#else
  ssize_t sent = send(ev->fd, buf, len, 0);

  if(sent < 0)
  {
    if(errno == EWOULDBLOCK)
      return 0;

    pony_throw();
  }

  return (size_t)sent;
#endif
}

size_t pony_os_recv(asio_event_t* ev, char* buf, size_t len)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_recv(ev, buf, len))
    pony_throw();

  return 0;
#else
  ssize_t received = recv(ev->fd, buf, len, 0);

  if(received < 0)
  {
    if(errno == EWOULDBLOCK)
      return 0;

    pony_throw();
  } else if(received == 0) {
    pony_throw();
  }

  return (size_t)received;
#endif
}

size_t pony_os_sendto(int fd, const char* buf, size_t len, ipaddress_t* ipaddr)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_sendto(fd, buf, len, ipaddr))
    pony_throw();

  return 0;
#else
  socklen_t addrlen = address_length(ipaddr);

  if(addrlen == (socklen_t)-1)
    pony_throw();

  ssize_t sent = sendto(fd, buf, len, 0, (struct sockaddr*)&ipaddr->addr,
    addrlen);

  if(sent < 0)
  {
    if(errno == EWOULDBLOCK)
      return 0;

    pony_throw();
  }

  return (size_t)sent;
#endif
}

size_t pony_os_recvfrom(asio_event_t* ev, char* buf, size_t len,
  ipaddress_t* ipaddr)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_recvfrom(ev, buf, len, ipaddr))
    pony_throw();

  return 0;
#else
  socklen_t addrlen = sizeof(struct sockaddr_storage);

  ssize_t recvd = recvfrom(ev->fd, (char*)buf, len, 0,
    (struct sockaddr*)&ipaddr->addr, &addrlen);

  if(recvd < 0)
  {
    if(errno == EWOULDBLOCK)
      return 0;

    pony_throw();
  } else if(recvd == 0) {
    pony_throw();
  }

  return (size_t)recvd;
#endif
}

void pony_os_keepalive(int fd, int secs)
{
  SOCKET s = (SOCKET)fd;

  int on = (secs > 0) ? 1 : 0;
  setsockopt(s, SOL_SOCKET,  SO_KEEPALIVE, (const char*)&on, sizeof(int));

  if(on == 0)
    return;

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)
  int probes = secs / 2;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(int));

  int idle = secs / 2;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int));

  int intvl = 1;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(int));
#elif defined(PLATFORM_IS_MACOSX)
  setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE, &secs, sizeof(int));
#elif defined(PLATFORM_IS_WINDOWS)
  DWORD ret = 0;

  struct tcp_keepalive k;
  k.onoff = 1;
  k.keepalivetime = secs / 2;
  k.keepaliveinterval = 1;

  WSAIoctl(s, SIO_KEEPALIVE_VALS, NULL, sizeof(struct tcp_keepalive), NULL, 0,
    &ret, NULL, NULL);
#endif
}

void pony_os_nodelay(int fd, bool state)
{
  int val = state;
  setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&val,
    sizeof(int));
}

void pony_os_socket_shutdown(int fd)
{
  shutdown((SOCKET)fd, 1);
}

void pony_os_socket_close(int fd)
{
#ifdef PLATFORM_IS_WINDOWS
  CancelIoEx((HANDLE)(UINT_PTR)fd, NULL);
  closesocket((SOCKET)fd);
#else
  close(fd);
#endif
}

bool ponyint_os_sockets_init()
{
#ifdef PLATFORM_IS_WINDOWS
  WORD ver = MAKEWORD(2, 2);
  WSADATA data;

  // Load the winsock library.
  int r = WSAStartup(ver, &data);

  if(r != 0)
    return false;

  // We need a fake socket in order to get the extension functions for IOCP.
  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  if(s == INVALID_SOCKET)
  {
    WSACleanup();
    return false;
  }

  GUID guid;
  DWORD dw;

  // Find ConnectEx.
  guid = WSAID_CONNECTEX;

  r = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
    &g_ConnectEx, sizeof(g_ConnectEx), &dw, NULL, NULL);

  if(r == SOCKET_ERROR)
  {
    closesocket(s);
    WSACleanup();
    return false;
  }

  // Find AcceptEx.
  guid = WSAID_ACCEPTEX;

  r = WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid),
    &g_AcceptEx, sizeof(g_AcceptEx), &dw, NULL, NULL);

  if(r == SOCKET_ERROR)
  {
    closesocket(s);
    WSACleanup();
    return false;
  }

  closesocket(s);
#endif

#ifdef PLATFORM_IS_POSIX_BASED
  // Ignore SIGPIPE to prevent writing to closed sockets, pipes, etc, from
  // raising a signal. If a program needs SIGPIPE, it can be accessed via the
  // signals package.
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if(sigaction(SIGPIPE, &sa, 0) == -1)
    return false;
#endif

  return true;
}

void ponyint_os_sockets_final()
{
#ifdef PLATFORM_IS_WINDOWS
  WSACleanup();
#endif
}

void pony_os_broadcast(int fd, bool state)
{
  int broadcast = state ? 1 : 0;
  setsockopt((SOCKET)fd, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast,
    sizeof(broadcast));
}

void pony_os_multicast_interface(int fd, const char* from)
{
  // Use the first reported address.
  struct addrinfo* p = os_addrinfo_intern(AF_UNSPEC, 0, 0, from, NULL, true);

  if(p != NULL)
  {
    setsockopt((SOCKET)fd, IPPROTO_IP, IP_MULTICAST_IF,
      (const char*)&p->ai_addr, (int)p->ai_addrlen);
    freeaddrinfo(p);
  }
}

void pony_os_multicast_loopback(int fd, bool loopback)
{
  uint8_t loop = loopback ? 1 : 0;
  setsockopt((SOCKET)fd, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loop,
    sizeof(loop));
}

void pony_os_multicast_ttl(int fd, uint8_t ttl)
{
  setsockopt((SOCKET)fd, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&ttl,
    sizeof(ttl));
}

static uint32_t multicast_interface(int family, const char* host)
{
  // Get a multicast interface for a host. For IPv4, this is an IP address.
  // For IPv6 this is an interface index number.
  if((host == NULL) || (host[0] == '\0'))
    return 0;

  struct addrinfo* p = os_addrinfo_intern(family, 0, 0, host, NULL, true);

  if(p == NULL)
    return 0;

  uint32_t interface = 0;

  switch(p->ai_family)
  {
    case AF_INET:
    {
      // Use the address instead of an interface number.
      interface = ((struct sockaddr_in*)p->ai_addr)->sin_addr.s_addr;
      break;
    }

    case AF_INET6:
    {
      // Use the sin6_scope_id as the interface number.
      interface = ((struct sockaddr_in6*)p->ai_addr)->sin6_scope_id;
      break;
    }

    default: {}
  }

  freeaddrinfo(p);
  return interface;
}

static void multicast_change(int fd, const char* group, const char* to,
  bool join)
{
  struct addrinfo* rg = os_addrinfo_intern(AF_UNSPEC, 0, 0, group, NULL, true);

  if(rg == NULL)
    return;

  uint32_t interface = multicast_interface(rg->ai_family, to);
  SOCKET s = (SOCKET)fd;

  switch(rg->ai_family)
  {
    case AF_INET:
    {
      struct ip_mreq req;

      req.imr_multiaddr = ((struct sockaddr_in*)rg->ai_addr)->sin_addr;
      req.imr_interface.s_addr = interface;

      if(join)
        setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&req,
          sizeof(req));
      else
        setsockopt(s, IPPROTO_IP, IP_DROP_MEMBERSHIP, (const char*)&req,
          sizeof(req));
      break;
    }

    case AF_INET6:
    {
      struct ipv6_mreq req;

      memcpy(&req.ipv6mr_multiaddr,
        &((struct sockaddr_in6*)rg->ai_addr)->sin6_addr,
        sizeof(struct in6_addr));

      req.ipv6mr_interface = interface;

      if(join)
        setsockopt(s, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char*)&req,
          sizeof(req));
      else
        setsockopt(s, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (const char*)&req,
          sizeof(req));
    }

    default: {}
  }

  freeaddrinfo(rg);
}

void pony_os_multicast_join(int fd, const char* group, const char* to)
{
  multicast_change(fd, group, to, true);
}

void pony_os_multicast_leave(int fd, const char* group, const char* to)
{
  multicast_change(fd, group, to, false);
}

PONY_EXTERN_C_END
