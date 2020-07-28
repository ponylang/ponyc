#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#include "../asio/asio.h"
#include "../asio/event.h"
#include "ponyassert.h"
#include <stdbool.h>
#include <string.h>

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

// headers for get/setsockopt constants
#ifdef  PLATFORM_IS_MACOSX
#include <net/if.h>
#include <net/ndrv.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#endif
#ifdef PLATFORM_IS_LINUX
#include <asm-generic/socket.h>
#include <linux/atm.h>
#include <linux/dn.h>
#include <linux/rds.h>
#if defined(__GLIBC__)
#include <netatalk/at.h>
#include <netax25/ax25.h>
#include <netax25/ax25.h>
#include <netipx/ipx.h>
#include <netrom/netrom.h>
#include <netrose/rose.h>
#endif
#include <linux/dccp.h>
#include <linux/netlink.h>
#include <linux/icmp.h>
#include <linux/tipc.h>
#include <linux/in6.h>
#include <linux/udp.h>
#endif
#ifdef PLATFORM_IS_BSD
#ifdef PLATFORM_IS_FREEBSD
#include <netinet/ip_mroute.h>
#include <netinet/sctp.h>
#elif defined(PLATFORM_IS_OPENBSD)
// Taken from FreeBSD
#define TCP_KEEPCNT 1024
#define TCP_KEEPIDLE 256
#define TCP_KEEPINTVL 512
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#endif
#ifdef PLATFORM_IS_WINDOWS
// TODO
#endif

PONY_EXTERN_C_BEGIN

PONY_API void pony_os_socket_close(int fd);

// This must match the pony NetAddress type in packages/net.
typedef struct
{
  pony_type_t* type;
  struct sockaddr_storage addr;
} ipaddress_t;

PONY_API socklen_t ponyint_address_length(ipaddress_t* ipaddr)
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

#if defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_BSD)
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
  socklen_t socklen = ponyint_address_length(ipaddr);

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
  pony_assert((skt == INVALID_SOCKET) || ((skt >> 31) == 0));
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

#if defined(PLATFORM_IS_MACOSX) || defined(PLATFORM_IS_BSD)
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
  const char* from, uint32_t asio_flags)
{
  map_any_to_loopback(p->ai_addr);

  bool need_bind = (from != NULL) && (from[0] != '\0');

  if(need_bind)
  {
    struct addrinfo* result = os_addrinfo_intern(p->ai_family, 0, 0, from,
      NULL, false);

    if(result == NULL)
      return false;

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
  asio_event_t* ev = pony_asio_event_create(owner, fd, asio_flags, 0, true);

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

  // Create an event and subscribe it.
  pony_asio_event_create(owner, fd, asio_flags, 0, true);
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

  if(result == NULL)
    return NULL;

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
  const char* service, const char* from, int family, int socktype, int proto,
  uint32_t asio_flags)
{
  bool reuse = (from == NULL) || (from[0] != '\0');

  struct addrinfo* result = os_addrinfo_intern(family, socktype, proto, host,
    service, false);

  if(result == NULL)
    return 0;

  struct addrinfo* p = result;
  int count = 0;

  while(p != NULL)
  {
    int fd = socket_from_addrinfo(p, reuse);

    if(fd != -1)
    {
      if(os_connect(owner, fd, p, from, asio_flags))
        count++;
    }

    p = p->ai_next;
  }

  freeaddrinfo(result);
  return count;
}

PONY_API asio_event_t* pony_os_listen_tcp(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_UNSPEC, SOCK_STREAM,
    IPPROTO_TCP);
}

PONY_API asio_event_t* pony_os_listen_tcp4(pony_actor_t* owner,
  const char* host, const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET, SOCK_STREAM,
    IPPROTO_TCP);
}

PONY_API asio_event_t* pony_os_listen_tcp6(pony_actor_t* owner,
  const char* host, const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET6, SOCK_STREAM,
    IPPROTO_TCP);
}

PONY_API asio_event_t* pony_os_listen_udp(pony_actor_t* owner, const char* host,
  const char* service)
{
  return os_socket_listen(owner, host, service, AF_UNSPEC, SOCK_DGRAM,
    IPPROTO_UDP);
}

PONY_API asio_event_t* pony_os_listen_udp4(pony_actor_t* owner,
  const char* host, const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET, SOCK_DGRAM,
    IPPROTO_UDP);
}

PONY_API asio_event_t* pony_os_listen_udp6(pony_actor_t* owner,
  const char* host, const char* service)
{
  return os_socket_listen(owner, host, service, AF_INET6, SOCK_DGRAM,
    IPPROTO_UDP);
}

PONY_API int pony_os_connect_tcp(pony_actor_t* owner, const char* host,
  const char* service, const char* from, uint32_t asio_flags)
{
  return os_socket_connect(owner, host, service, from, AF_UNSPEC, SOCK_STREAM,
    IPPROTO_TCP, asio_flags);
}

PONY_API int pony_os_connect_tcp4(pony_actor_t* owner, const char* host,
  const char* service, const char* from, uint32_t asio_flags)
{
  return os_socket_connect(owner, host, service, from, AF_INET, SOCK_STREAM,
    IPPROTO_TCP, asio_flags);
}

PONY_API int pony_os_connect_tcp6(pony_actor_t* owner, const char* host,
  const char* service, const char* from, uint32_t asio_flags)
{
  return os_socket_connect(owner, host, service, from, AF_INET6, SOCK_STREAM,
    IPPROTO_TCP, asio_flags);
}

PONY_API int pony_os_accept(asio_event_t* ev)
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
// API deprecated: use TCPConnection._is_sock_connected or UDPSocket.get_so_error
PONY_API bool pony_os_connected(int fd)
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

PONY_API bool pony_os_nameinfo(ipaddress_t* ipaddr, char** rhost, char** rserv,
  bool reversedns, bool servicename)
{
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];

  socklen_t len = ponyint_address_length(ipaddr);

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

PONY_API struct addrinfo* pony_os_addrinfo(int family, const char* host,
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

PONY_API void pony_os_getaddr(struct addrinfo* addr, ipaddress_t* ipaddr)
{
  memcpy(&ipaddr->addr, addr->ai_addr, addr->ai_addrlen);
  map_any_to_loopback((struct sockaddr*)&ipaddr->addr);
}

PONY_API struct addrinfo* pony_os_nextaddr(struct addrinfo* addr)
{
  return addr->ai_next;
}

PONY_API char* pony_os_ip_string(void* src, int len)
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

PONY_API bool pony_os_ipv4(ipaddress_t* ipaddr)
{
  return ipaddr->addr.ss_family == AF_INET;
}

PONY_API bool pony_os_ipv6(ipaddress_t* ipaddr)
{
  return ipaddr->addr.ss_family == AF_INET6;
}

PONY_API bool pony_os_sockname(int fd, ipaddress_t* ipaddr)
{
  socklen_t len = sizeof(struct sockaddr_storage);

  if(getsockname((SOCKET)fd, (struct sockaddr*)&ipaddr->addr, &len) != 0)
    return false;

  map_any_to_loopback((struct sockaddr*)&ipaddr->addr);
  return true;
}

PONY_API bool pony_os_peername(int fd, ipaddress_t* ipaddr)
{
  socklen_t len = sizeof(struct sockaddr_storage);

  if(getpeername((SOCKET)fd, (struct sockaddr*)&ipaddr->addr, &len) != 0)
    return false;

  map_any_to_loopback((struct sockaddr*)&ipaddr->addr);
  return true;
}

PONY_API bool pony_os_host_ip4(const char* host)
{
  struct in_addr addr;
  return inet_pton(AF_INET, host, &addr) == 1;
}

PONY_API bool pony_os_host_ip6(const char* host)
{
  struct in6_addr addr;
  return inet_pton(AF_INET6, host, &addr) == 1;
}

#ifdef PLATFORM_IS_WINDOWS
PONY_API size_t pony_os_writev(asio_event_t* ev, LPWSABUF wsa, int wsacnt)
{
  SOCKET s = (SOCKET)ev->fd;
  iocp_t* iocp = iocp_create(IOCP_SEND, ev);
  DWORD sent;

  if(WSASend(s, wsa, wsacnt, &sent, 0, &iocp->ov, NULL) != 0)
  {
    if(GetLastError() != WSA_IO_PENDING)
    {
      iocp_destroy(iocp);
      pony_error();
    }
  }

  return 0;
}
#else
PONY_API size_t pony_os_writev(asio_event_t* ev, const struct iovec *iov, int iovcnt)
{
  ssize_t sent = writev(ev->fd, iov, iovcnt);

  if(sent < 0)
  {
    if(errno == EWOULDBLOCK || errno == EAGAIN)
      return 0;

    pony_error();
  }

  return (size_t)sent;
}
#endif

PONY_API size_t pony_os_send(asio_event_t* ev, const char* buf, size_t len)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_send(ev, buf, len))
    pony_error();

  return 0;
#else
  ssize_t sent = send(ev->fd, buf, len, 0);

  if(sent < 0)
  {
    if(errno == EWOULDBLOCK || errno == EAGAIN)
      return 0;

    pony_error();
  }

  return (size_t)sent;
#endif
}

PONY_API size_t pony_os_recv(asio_event_t* ev, char* buf, size_t len)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_recv(ev, buf, len))
    pony_error();

  return 0;
#else
  ssize_t received = recv(ev->fd, buf, len, 0);

  if(received < 0)
  {
    if(errno == EWOULDBLOCK || errno == EAGAIN)
      return 0;

    pony_error();
  } else if(received == 0) {
    pony_error();
  }

  return (size_t)received;
#endif
}

PONY_API size_t pony_os_sendto(int fd, const char* buf, size_t len,
  ipaddress_t* ipaddr)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_sendto(fd, buf, len, ipaddr))
    pony_error();

  return 0;
#else
  socklen_t addrlen = ponyint_address_length(ipaddr);

  if(addrlen == (socklen_t)-1)
    pony_error();

  ssize_t sent = sendto(fd, buf, len, 0, (struct sockaddr*)&ipaddr->addr,
    addrlen);

  if(sent < 0)
  {
    if(errno == EWOULDBLOCK || errno == EAGAIN)
      return 0;

    pony_error();
  }

  return (size_t)sent;
#endif
}

PONY_API size_t pony_os_recvfrom(asio_event_t* ev, char* buf, size_t len,
  ipaddress_t* ipaddr)
{
#ifdef PLATFORM_IS_WINDOWS
  if(!iocp_recvfrom(ev, buf, len, ipaddr))
    pony_error();

  return 0;
#else
  socklen_t addrlen = sizeof(struct sockaddr_storage);

  ssize_t recvd = recvfrom(ev->fd, (char*)buf, len, 0,
    (struct sockaddr*)&ipaddr->addr, &addrlen);

  if(recvd < 0)
  {
    if(errno == EWOULDBLOCK || errno == EAGAIN)
      return 0;

    pony_error();
  } else if(recvd == 0) {
    pony_error();
  }

  return (size_t)recvd;
#endif
}

PONY_API void pony_os_keepalive(int fd, int secs)
{
  SOCKET s = (SOCKET)fd;

  int on = (secs > 0) ? 1 : 0;
  setsockopt(s, SOL_SOCKET,  SO_KEEPALIVE, (const char*)&on, sizeof(int));

  if(on == 0)
    return;

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_BSD) || defined(PLATFORM_IS_MACOSX)
  int probes = secs / 2;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPCNT, &probes, sizeof(int));

  int idle = secs / 2;
#if defined(PLATFORM_IS_MACOSX)
  setsockopt(s, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(int));
#else
  setsockopt(s, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int));
#endif

  int intvl = 1;
  setsockopt(s, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(int));
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

// API deprecated: use TCPConnection.set_tcp_nodelay
PONY_API void pony_os_nodelay(int fd, bool state)
{
  int val = state;

  setsockopt((SOCKET)fd, IPPROTO_TCP, TCP_NODELAY, (const char*)&val,
    sizeof(int));
}

PONY_API void pony_os_socket_shutdown(int fd)
{
  shutdown((SOCKET)fd, 1);
}

PONY_API void pony_os_socket_close(int fd)
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

// API deprecated: use UDPSocket.set_so_broadcast
PONY_API void pony_os_broadcast(int fd, bool state)
{
  int broadcast = state ? 1 : 0;

  setsockopt((SOCKET)fd, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast,
    sizeof(broadcast));
}

PONY_API void pony_os_multicast_interface(int fd, const char* from)
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

// API deprecated: use UDPSocket.set_ip_multicast_loop
PONY_API void pony_os_multicast_loopback(int fd, bool loopback)
{
  uint8_t loop = loopback ? 1 : 0;

  setsockopt((SOCKET)fd, IPPROTO_IP, IP_MULTICAST_LOOP, (const char*)&loop,
    sizeof(loop));
}

// API deprecated: use UDPSocket.set_ip_multicast_ttl
PONY_API void pony_os_multicast_ttl(int fd, uint8_t ttl)
{
  setsockopt((SOCKET)fd, IPPROTO_IP, IP_MULTICAST_TTL, (const char*)&ttl,
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

PONY_API void pony_os_multicast_join(int fd, const char* group, const char* to)
{
  multicast_change(fd, group, to, true);
}

PONY_API void pony_os_multicast_leave(int fd, const char* group, const char* to)
{
  multicast_change(fd, group, to, false);
}

/* Constants are from
 *   macOS Sierra 10.12.6
 *   Ubuntu Linux Xenial/16.04 LTS + kernel 4.4.0-109-generic
 *   FreeBSD 11.1-RELEASE
 *   Windows Winsock function reference for getsockopt & setsockopt:
 *     https://msdn.microsoft.com/en-us/library/windows/desktop/ms738544(v=vs.85).aspx
 *     https://msdn.microsoft.com/en-us/library/windows/desktop/ms740476(v=vs.85).aspx
 * Harvested by (except Windows):
 *   egrep -s '\b(_AX25|DCCP|DSO|ICMP|IPSEC|IPT|IPX|IP[A-Z0-6]*|LOCAL|MCAST[A-Z0-6]*|MRT|NDRV|NETLINK|NETROM|RDS|ROSE|SCO|SCTP|SO|SOL|TCP[A-Z0-6]*|TIPC|UDP[A-Z0-6]*)' /usr/include/asm-generic/socket.h /usr/include/linux/atm.h /usr/include/linux/dccp.h /usr/include/linux/dn.h /usr/include/linux/icmp.h /usr/include/linux/in.h /usr/include/linux/in6.h /usr/include/linux/netfilter_ipv4.h /usr/include/linux/netlink.h /usr/include/linux/rds.h /usr/include/linux/tcp.h /usr/include/linux/tipc.h /usr/include/linux/udp.h /usr/include/linux/vm_sockets.h /usr/include/net/ndrv.h /usr/include/netatalk/at.h /usr/include/netax25/ax25.h /usr/include/netfilter_ipv4/ip_tables.h /usr/include/netfilter_ipv6/ip6_tables.h /usr/include/netgraph/bluetooth/include/ng_btsocket.h /usr/include/netinet/in.h /usr/include/netinet/ip_mroute.h /usr/include/netinet/sctp.h /usr/include/netinet/tcp.h /usr/include/netinet/udp.h /usr/include/netipx/ipx.h /usr/include/netrom/netrom.h /usr/include/netrose/rose.h /usr/include/sys/socket.h /usr/include/sys/un.h | egrep -v 'bad-macros-filtered-here|SO_CIRANGE' | egrep '#.*define' | awk '{print $2}' | sort -u
 *
 * These constants are _not_ stable between Pony releases.
 * Values returned by this function may be held by long-lived variables
 * by the calling process: values cannot change while the process runs.
 * Programmers must not cache any of these values for purposes of
 * sharing them for use by any other Pony program (for example,
 * sharing via serialization & deserialization or via direct
 * shared memory).
 */

PONY_API int pony_os_sockopt_level(int option)
{
  switch (option)
  {
  /*
   * Formatted in C by:
   *   egrep '^(IP[A-Z0-6]*PROTO_|NSPROTO_|SOL_)' ~/sum-of-all-constants.txt | egrep -v '\(' | sort -u | egrep -v '^$' | awk 'BEGIN { count=4000; } { printf("#ifdef %s\n    case %2d: return %s;\n#endif\n", $1, count++, $1); }'
   */
#ifdef IPPROTO_3PC
    case 4000: return IPPROTO_3PC;
#endif
#ifdef IPPROTO_ADFS
    case 4001: return IPPROTO_ADFS;
#endif
#ifdef IPPROTO_AH
    case 4002: return IPPROTO_AH;
#endif
#ifdef IPPROTO_AHIP
    case 4003: return IPPROTO_AHIP;
#endif
#ifdef IPPROTO_APES
    case 4004: return IPPROTO_APES;
#endif
#ifdef IPPROTO_ARGUS
    case 4005: return IPPROTO_ARGUS;
#endif
#ifdef IPPROTO_AX25
    case 4006: return IPPROTO_AX25;
#endif
#ifdef IPPROTO_BEETPH
    case 4007: return IPPROTO_BEETPH;
#endif
#ifdef IPPROTO_BHA
    case 4008: return IPPROTO_BHA;
#endif
#ifdef IPPROTO_BLT
    case 4009: return IPPROTO_BLT;
#endif
#ifdef IPPROTO_BRSATMON
    case 4010: return IPPROTO_BRSATMON;
#endif
#ifdef IPPROTO_CARP
    case 4011: return IPPROTO_CARP;
#endif
#ifdef IPPROTO_CFTP
    case 4012: return IPPROTO_CFTP;
#endif
#ifdef IPPROTO_CHAOS
    case 4013: return IPPROTO_CHAOS;
#endif
#ifdef IPPROTO_CMTP
    case 4014: return IPPROTO_CMTP;
#endif
#ifdef IPPROTO_COMP
    case 4015: return IPPROTO_COMP;
#endif
#ifdef IPPROTO_CPHB
    case 4016: return IPPROTO_CPHB;
#endif
#ifdef IPPROTO_CPNX
    case 4017: return IPPROTO_CPNX;
#endif
#ifdef IPPROTO_DCCP
    case 4018: return IPPROTO_DCCP;
#endif
#ifdef IPPROTO_DDP
    case 4019: return IPPROTO_DDP;
#endif
#ifdef IPPROTO_DGP
    case 4020: return IPPROTO_DGP;
#endif
#ifdef IPPROTO_DIVERT
    case 4021: return IPPROTO_DIVERT;
#endif
#ifdef IPPROTO_DONE
    case 4022: return IPPROTO_DONE;
#endif
#ifdef IPPROTO_DSTOPTS
    case 4023: return IPPROTO_DSTOPTS;
#endif
#ifdef IPPROTO_EGP
    case 4024: return IPPROTO_EGP;
#endif
#ifdef IPPROTO_EMCON
    case 4025: return IPPROTO_EMCON;
#endif
#ifdef IPPROTO_ENCAP
    case 4026: return IPPROTO_ENCAP;
#endif
#ifdef IPPROTO_EON
    case 4027: return IPPROTO_EON;
#endif
#ifdef IPPROTO_ESP
    case 4028: return IPPROTO_ESP;
#endif
#ifdef IPPROTO_ETHERIP
    case 4029: return IPPROTO_ETHERIP;
#endif
#ifdef IPPROTO_FRAGMENT
    case 4030: return IPPROTO_FRAGMENT;
#endif
#ifdef IPPROTO_GGP
    case 4031: return IPPROTO_GGP;
#endif
#ifdef IPPROTO_GMTP
    case 4032: return IPPROTO_GMTP;
#endif
#ifdef IPPROTO_GRE
    case 4033: return IPPROTO_GRE;
#endif
#ifdef IPPROTO_HELLO
    case 4034: return IPPROTO_HELLO;
#endif
#ifdef IPPROTO_HIP
    case 4035: return IPPROTO_HIP;
#endif
#ifdef IPPROTO_HMP
    case 4036: return IPPROTO_HMP;
#endif
#ifdef IPPROTO_HOPOPTS
    case 4037: return IPPROTO_HOPOPTS;
#endif
#ifdef IPPROTO_ICMP
    case 4038: return IPPROTO_ICMP;
#endif
#ifdef IPPROTO_ICMPV6
    case 4039: return IPPROTO_ICMPV6;
#endif
#ifdef IPPROTO_IDP
    case 4040: return IPPROTO_IDP;
#endif
#ifdef IPPROTO_IDPR
    case 4041: return IPPROTO_IDPR;
#endif
#ifdef IPPROTO_IDRP
    case 4042: return IPPROTO_IDRP;
#endif
#ifdef IPPROTO_IGMP
    case 4043: return IPPROTO_IGMP;
#endif
#ifdef IPPROTO_IGP
    case 4044: return IPPROTO_IGP;
#endif
#ifdef IPPROTO_IGRP
    case 4045: return IPPROTO_IGRP;
#endif
#ifdef IPPROTO_IL
    case 4046: return IPPROTO_IL;
#endif
#ifdef IPPROTO_INLSP
    case 4047: return IPPROTO_INLSP;
#endif
#ifdef IPPROTO_INP
    case 4048: return IPPROTO_INP;
#endif
#ifdef IPPROTO_IP
    case 4049: return IPPROTO_IP;
#endif
#ifdef IPPROTO_IPCOMP
    case 4050: return IPPROTO_IPCOMP;
#endif
#ifdef IPPROTO_IPCV
    case 4051: return IPPROTO_IPCV;
#endif
#ifdef IPPROTO_IPEIP
    case 4052: return IPPROTO_IPEIP;
#endif
#ifdef IPPROTO_IPIP
    case 4053: return IPPROTO_IPIP;
#endif
#ifdef IPPROTO_IPPC
    case 4054: return IPPROTO_IPPC;
#endif
#ifdef IPPROTO_IPV4
    case 4055: return IPPROTO_IPV4;
#endif
#ifdef IPPROTO_IPV6
    case 4056: return IPPROTO_IPV6;
#endif
#ifdef IPPROTO_IRTP
    case 4057: return IPPROTO_IRTP;
#endif
#ifdef IPPROTO_KRYPTOLAN
    case 4058: return IPPROTO_KRYPTOLAN;
#endif
#ifdef IPPROTO_LARP
    case 4059: return IPPROTO_LARP;
#endif
#ifdef IPPROTO_LEAF1
    case 4060: return IPPROTO_LEAF1;
#endif
#ifdef IPPROTO_LEAF2
    case 4061: return IPPROTO_LEAF2;
#endif
#ifdef IPPROTO_MAX
    case 4062: return IPPROTO_MAX;
#endif
#ifdef IPPROTO_MAXID
    case 4063: return IPPROTO_MAXID;
#endif
#ifdef IPPROTO_MEAS
    case 4064: return IPPROTO_MEAS;
#endif
#ifdef IPPROTO_MH
    case 4065: return IPPROTO_MH;
#endif
#ifdef IPPROTO_MHRP
    case 4066: return IPPROTO_MHRP;
#endif
#ifdef IPPROTO_MICP
    case 4067: return IPPROTO_MICP;
#endif
#ifdef IPPROTO_MOBILE
    case 4068: return IPPROTO_MOBILE;
#endif
#ifdef IPPROTO_MPLS
    case 4069: return IPPROTO_MPLS;
#endif
#ifdef IPPROTO_MTP
    case 4070: return IPPROTO_MTP;
#endif
#ifdef IPPROTO_MUX
    case 4071: return IPPROTO_MUX;
#endif
#ifdef IPPROTO_ND
    case 4072: return IPPROTO_ND;
#endif
#ifdef IPPROTO_NHRP
    case 4073: return IPPROTO_NHRP;
#endif
#ifdef IPPROTO_NONE
    case 4074: return IPPROTO_NONE;
#endif
#ifdef IPPROTO_NSP
    case 4075: return IPPROTO_NSP;
#endif
#ifdef IPPROTO_NVPII
    case 4076: return IPPROTO_NVPII;
#endif
#ifdef IPPROTO_OLD_DIVERT
    case 4077: return IPPROTO_OLD_DIVERT;
#endif
#ifdef IPPROTO_OSPFIGP
    case 4078: return IPPROTO_OSPFIGP;
#endif
#ifdef IPPROTO_PFSYNC
    case 4079: return IPPROTO_PFSYNC;
#endif
#ifdef IPPROTO_PGM
    case 4080: return IPPROTO_PGM;
#endif
#ifdef IPPROTO_PIGP
    case 4081: return IPPROTO_PIGP;
#endif
#ifdef IPPROTO_PIM
    case 4082: return IPPROTO_PIM;
#endif
#ifdef IPPROTO_PRM
    case 4083: return IPPROTO_PRM;
#endif
#ifdef IPPROTO_PUP
    case 4084: return IPPROTO_PUP;
#endif
#ifdef IPPROTO_PVP
    case 4085: return IPPROTO_PVP;
#endif
#ifdef IPPROTO_RAW
    case 4086: return IPPROTO_RAW;
#endif
#ifdef IPPROTO_RCCMON
    case 4087: return IPPROTO_RCCMON;
#endif
#ifdef IPPROTO_RDP
    case 4088: return IPPROTO_RDP;
#endif
#ifdef IPPROTO_RESERVED_253
    case 4089: return IPPROTO_RESERVED_253;
#endif
#ifdef IPPROTO_RESERVED_254
    case 4090: return IPPROTO_RESERVED_254;
#endif
#ifdef IPPROTO_ROUTING
    case 4091: return IPPROTO_ROUTING;
#endif
#ifdef IPPROTO_RSVP
    case 4092: return IPPROTO_RSVP;
#endif
#ifdef IPPROTO_RVD
    case 4093: return IPPROTO_RVD;
#endif
#ifdef IPPROTO_SATEXPAK
    case 4094: return IPPROTO_SATEXPAK;
#endif
#ifdef IPPROTO_SATMON
    case 4095: return IPPROTO_SATMON;
#endif
#ifdef IPPROTO_SCCSP
    case 4096: return IPPROTO_SCCSP;
#endif
#ifdef IPPROTO_SCTP
    case 4097: return IPPROTO_SCTP;
#endif
#ifdef IPPROTO_SDRP
    case 4098: return IPPROTO_SDRP;
#endif
#ifdef IPPROTO_SEND
    case 4099: return IPPROTO_SEND;
#endif
#ifdef IPPROTO_SEP
    case 4100: return IPPROTO_SEP;
#endif
#ifdef IPPROTO_SHIM6
    case 4101: return IPPROTO_SHIM6;
#endif
#ifdef IPPROTO_SKIP
    case 4102: return IPPROTO_SKIP;
#endif
#ifdef IPPROTO_SPACER
    case 4103: return IPPROTO_SPACER;
#endif
#ifdef IPPROTO_SRPC
    case 4104: return IPPROTO_SRPC;
#endif
#ifdef IPPROTO_ST
    case 4105: return IPPROTO_ST;
#endif
#ifdef IPPROTO_SVMTP
    case 4106: return IPPROTO_SVMTP;
#endif
#ifdef IPPROTO_SWIPE
    case 4107: return IPPROTO_SWIPE;
#endif
#ifdef IPPROTO_TCF
    case 4108: return IPPROTO_TCF;
#endif
#ifdef IPPROTO_TCP
    case 4109: return IPPROTO_TCP;
#endif
#ifdef IPPROTO_TLSP
    case 4110: return IPPROTO_TLSP;
#endif
#ifdef IPPROTO_TP
    case 4111: return IPPROTO_TP;
#endif
#ifdef IPPROTO_TPXX
    case 4112: return IPPROTO_TPXX;
#endif
#ifdef IPPROTO_TRUNK1
    case 4113: return IPPROTO_TRUNK1;
#endif
#ifdef IPPROTO_TRUNK2
    case 4114: return IPPROTO_TRUNK2;
#endif
#ifdef IPPROTO_TTP
    case 4115: return IPPROTO_TTP;
#endif
#ifdef IPPROTO_UDP
    case 4116: return IPPROTO_UDP;
#endif
#ifdef IPPROTO_UDPLITE
    case 4117: return IPPROTO_UDPLITE;
#endif
#ifdef IPPROTO_VINES
    case 4118: return IPPROTO_VINES;
#endif
#ifdef IPPROTO_VISA
    case 4119: return IPPROTO_VISA;
#endif
#ifdef IPPROTO_VMTP
    case 4120: return IPPROTO_VMTP;
#endif
#ifdef IPPROTO_WBEXPAK
    case 4121: return IPPROTO_WBEXPAK;
#endif
#ifdef IPPROTO_WBMON
    case 4122: return IPPROTO_WBMON;
#endif
#ifdef IPPROTO_WSN
    case 4123: return IPPROTO_WSN;
#endif
#ifdef IPPROTO_XNET
    case 4124: return IPPROTO_XNET;
#endif
#ifdef IPPROTO_XTP
    case 4125: return IPPROTO_XTP;
#endif
#ifdef SOL_ATALK
    case 4126: return SOL_ATALK;
#endif
#ifdef SOL_AX25
    case 4127: return SOL_AX25;
#endif
#ifdef SOL_HCI_RAW
    case 4128: return SOL_HCI_RAW;
#endif
#ifdef SOL_IPX
    case 4129: return SOL_IPX;
#endif
#ifdef SOL_L2CAP
    case 4130: return SOL_L2CAP;
#endif
#ifdef SOL_LOCAL
    case 4131: return SOL_LOCAL;
#endif
#ifdef SOL_NDRVPROTO
    case 4132: return SOL_NDRVPROTO;
#endif
#ifdef SOL_NETROM
    case 4133: return SOL_NETROM;
#endif
#ifdef SOL_RDS
    case 4134: return SOL_RDS;
#endif
#ifdef SOL_RFCOMM
    case 4135: return SOL_RFCOMM;
#endif
#ifdef SOL_ROSE
    case 4136: return SOL_ROSE;
#endif
#ifdef SOL_SCO
    case 4137: return SOL_SCO;
#endif
#ifdef SOL_SOCKET
    case 4138: return SOL_SOCKET;
#endif
#ifdef SOL_TIPC
    case 4139: return SOL_TIPC;
#endif
#ifdef SOL_UDP
    case 4140: return SOL_UDP;
#endif
    default: return -1;
  }
}

/*
 * These constants are _not_ stable between Pony releases.
 * Values returned by this function may be held by long-lived variables
 * by the calling process: values cannot change while the process runs.
 * Programmers must not cache any of these values for purposes of
 * sharing them for use by any other Pony program (for example,
 * sharing via serialization & deserialization or via direct
 * shared memory).
 */

PONY_API int pony_os_sockopt_option(int option)
{
  switch (option)
  {
  /*
   * Formatted in C by:
   *   egrep -v '^(IP[A-Z0-6]*PROTO_|NSPROTO_|SOL_)' ~/sum-of-all-constants.txt | egrep -v '\(' | sort -u | egrep -v '^$' | awk '{ printf("#ifdef %s\n    case %2d: return %s;\n#endif\n", $1, count++, $1); }'
   */
#ifdef AF_COIP
    case  0: return AF_COIP;
#endif
#ifdef AF_INET
    case  1: return AF_INET;
#endif
#ifdef AF_INET6
    case  2: return AF_INET6;
#endif
#ifdef BLUETOOTH_PROTO_SCO
    case  3: return BLUETOOTH_PROTO_SCO;
#endif
#ifdef DCCP_NR_PKT_TYPES
    case  4: return DCCP_NR_PKT_TYPES;
#endif
#ifdef DCCP_SERVICE_LIST_MAX_LEN
    case  5: return DCCP_SERVICE_LIST_MAX_LEN;
#endif
#ifdef DCCP_SINGLE_OPT_MAXLEN
    case  6: return DCCP_SINGLE_OPT_MAXLEN;
#endif
#ifdef DCCP_SOCKOPT_AVAILABLE_CCIDS
    case  7: return DCCP_SOCKOPT_AVAILABLE_CCIDS;
#endif
#ifdef DCCP_SOCKOPT_CCID
    case  8: return DCCP_SOCKOPT_CCID;
#endif
#ifdef DCCP_SOCKOPT_CCID_RX_INFO
    case  9: return DCCP_SOCKOPT_CCID_RX_INFO;
#endif
#ifdef DCCP_SOCKOPT_CCID_TX_INFO
    case 10: return DCCP_SOCKOPT_CCID_TX_INFO;
#endif
#ifdef DCCP_SOCKOPT_CHANGE_L
    case 11: return DCCP_SOCKOPT_CHANGE_L;
#endif
#ifdef DCCP_SOCKOPT_CHANGE_R
    case 12: return DCCP_SOCKOPT_CHANGE_R;
#endif
#ifdef DCCP_SOCKOPT_GET_CUR_MPS
    case 13: return DCCP_SOCKOPT_GET_CUR_MPS;
#endif
#ifdef DCCP_SOCKOPT_PACKET_SIZE
    case 14: return DCCP_SOCKOPT_PACKET_SIZE;
#endif
#ifdef DCCP_SOCKOPT_QPOLICY_ID
    case 15: return DCCP_SOCKOPT_QPOLICY_ID;
#endif
#ifdef DCCP_SOCKOPT_QPOLICY_TXQLEN
    case 16: return DCCP_SOCKOPT_QPOLICY_TXQLEN;
#endif
#ifdef DCCP_SOCKOPT_RECV_CSCOV
    case 17: return DCCP_SOCKOPT_RECV_CSCOV;
#endif
#ifdef DCCP_SOCKOPT_RX_CCID
    case 18: return DCCP_SOCKOPT_RX_CCID;
#endif
#ifdef DCCP_SOCKOPT_SEND_CSCOV
    case 19: return DCCP_SOCKOPT_SEND_CSCOV;
#endif
#ifdef DCCP_SOCKOPT_SERVER_TIMEWAIT
    case 20: return DCCP_SOCKOPT_SERVER_TIMEWAIT;
#endif
#ifdef DCCP_SOCKOPT_SERVICE
    case 21: return DCCP_SOCKOPT_SERVICE;
#endif
#ifdef DCCP_SOCKOPT_TX_CCID
    case 22: return DCCP_SOCKOPT_TX_CCID;
#endif
#ifdef DSO_ACCEPTMODE
    case 23: return DSO_ACCEPTMODE;
#endif
#ifdef DSO_CONACCEPT
    case 24: return DSO_CONACCEPT;
#endif
#ifdef DSO_CONACCESS
    case 25: return DSO_CONACCESS;
#endif
#ifdef DSO_CONDATA
    case 26: return DSO_CONDATA;
#endif
#ifdef DSO_CONREJECT
    case 27: return DSO_CONREJECT;
#endif
#ifdef DSO_CORK
    case 28: return DSO_CORK;
#endif
#ifdef DSO_DISDATA
    case 29: return DSO_DISDATA;
#endif
#ifdef DSO_INFO
    case 30: return DSO_INFO;
#endif
#ifdef DSO_LINKINFO
    case 31: return DSO_LINKINFO;
#endif
#ifdef DSO_MAX
    case 32: return DSO_MAX;
#endif
#ifdef DSO_MAXWINDOW
    case 33: return DSO_MAXWINDOW;
#endif
#ifdef DSO_NODELAY
    case 34: return DSO_NODELAY;
#endif
#ifdef DSO_SEQPACKET
    case 35: return DSO_SEQPACKET;
#endif
#ifdef DSO_SERVICES
    case 36: return DSO_SERVICES;
#endif
#ifdef DSO_STREAM
    case 37: return DSO_STREAM;
#endif
#ifdef ICMP_ADDRESS
    case 38: return ICMP_ADDRESS;
#endif
#ifdef ICMP_ADDRESSREPLY
    case 39: return ICMP_ADDRESSREPLY;
#endif
#ifdef ICMP_DEST_UNREACH
    case 40: return ICMP_DEST_UNREACH;
#endif
#ifdef ICMP_ECHO
    case 41: return ICMP_ECHO;
#endif
#ifdef ICMP_ECHOREPLY
    case 42: return ICMP_ECHOREPLY;
#endif
#ifdef ICMP_EXC_FRAGTIME
    case 43: return ICMP_EXC_FRAGTIME;
#endif
#ifdef ICMP_EXC_TTL
    case 44: return ICMP_EXC_TTL;
#endif
#ifdef ICMP_FILTER
    case 45: return ICMP_FILTER;
#endif
#ifdef ICMP_FRAG_NEEDED
    case 46: return ICMP_FRAG_NEEDED;
#endif
#ifdef ICMP_HOST_ANO
    case 47: return ICMP_HOST_ANO;
#endif
#ifdef ICMP_HOST_ISOLATED
    case 48: return ICMP_HOST_ISOLATED;
#endif
#ifdef ICMP_HOST_UNKNOWN
    case 49: return ICMP_HOST_UNKNOWN;
#endif
#ifdef ICMP_HOST_UNREACH
    case 50: return ICMP_HOST_UNREACH;
#endif
#ifdef ICMP_HOST_UNR_TOS
    case 51: return ICMP_HOST_UNR_TOS;
#endif
#ifdef ICMP_INFO_REPLY
    case 52: return ICMP_INFO_REPLY;
#endif
#ifdef ICMP_INFO_REQUEST
    case 53: return ICMP_INFO_REQUEST;
#endif
#ifdef ICMP_NET_ANO
    case 54: return ICMP_NET_ANO;
#endif
#ifdef ICMP_NET_UNKNOWN
    case 55: return ICMP_NET_UNKNOWN;
#endif
#ifdef ICMP_NET_UNREACH
    case 56: return ICMP_NET_UNREACH;
#endif
#ifdef ICMP_NET_UNR_TOS
    case 57: return ICMP_NET_UNR_TOS;
#endif
#ifdef ICMP_PARAMETERPROB
    case 58: return ICMP_PARAMETERPROB;
#endif
#ifdef ICMP_PKT_FILTERED
    case 59: return ICMP_PKT_FILTERED;
#endif
#ifdef ICMP_PORT_UNREACH
    case 60: return ICMP_PORT_UNREACH;
#endif
#ifdef ICMP_PREC_CUTOFF
    case 61: return ICMP_PREC_CUTOFF;
#endif
#ifdef ICMP_PREC_VIOLATION
    case 62: return ICMP_PREC_VIOLATION;
#endif
#ifdef ICMP_PROT_UNREACH
    case 63: return ICMP_PROT_UNREACH;
#endif
#ifdef ICMP_REDIRECT
    case 64: return ICMP_REDIRECT;
#endif
#ifdef ICMP_REDIR_HOST
    case 65: return ICMP_REDIR_HOST;
#endif
#ifdef ICMP_REDIR_HOSTTOS
    case 66: return ICMP_REDIR_HOSTTOS;
#endif
#ifdef ICMP_REDIR_NET
    case 67: return ICMP_REDIR_NET;
#endif
#ifdef ICMP_REDIR_NETTOS
    case 68: return ICMP_REDIR_NETTOS;
#endif
#ifdef ICMP_SOURCE_QUENCH
    case 69: return ICMP_SOURCE_QUENCH;
#endif
#ifdef ICMP_SR_FAILED
    case 70: return ICMP_SR_FAILED;
#endif
#ifdef ICMP_TIMESTAMP
    case 71: return ICMP_TIMESTAMP;
#endif
#ifdef ICMP_TIMESTAMPREPLY
    case 72: return ICMP_TIMESTAMPREPLY;
#endif
#ifdef ICMP_TIME_EXCEEDED
    case 73: return ICMP_TIME_EXCEEDED;
#endif
#ifdef IPCTL_ACCEPTSOURCEROUTE
    case 74: return IPCTL_ACCEPTSOURCEROUTE;
#endif
#ifdef IPCTL_DEFMTU
    case 75: return IPCTL_DEFMTU;
#endif
#ifdef IPCTL_DEFTTL
    case 76: return IPCTL_DEFTTL;
#endif
#ifdef IPCTL_DIRECTEDBROADCAST
    case 77: return IPCTL_DIRECTEDBROADCAST;
#endif
#ifdef IPCTL_FASTFORWARDING
    case 78: return IPCTL_FASTFORWARDING;
#endif
#ifdef IPCTL_FORWARDING
    case 79: return IPCTL_FORWARDING;
#endif
#ifdef IPCTL_GIF_TTL
    case 80: return IPCTL_GIF_TTL;
#endif
#ifdef IPCTL_INTRDQDROPS
    case 81: return IPCTL_INTRDQDROPS;
#endif
#ifdef IPCTL_INTRDQMAXLEN
    case 82: return IPCTL_INTRDQMAXLEN;
#endif
#ifdef IPCTL_INTRQDROPS
    case 83: return IPCTL_INTRQDROPS;
#endif
#ifdef IPCTL_INTRQMAXLEN
    case 84: return IPCTL_INTRQMAXLEN;
#endif
#ifdef IPCTL_KEEPFAITH
    case 85: return IPCTL_KEEPFAITH;
#endif
#ifdef IPCTL_MAXID
    case 86: return IPCTL_MAXID;
#endif
#ifdef IPCTL_RTEXPIRE
    case 87: return IPCTL_RTEXPIRE;
#endif
#ifdef IPCTL_RTMAXCACHE
    case 88: return IPCTL_RTMAXCACHE;
#endif
#ifdef IPCTL_RTMINEXPIRE
    case 89: return IPCTL_RTMINEXPIRE;
#endif
#ifdef IPCTL_SENDREDIRECTS
    case 90: return IPCTL_SENDREDIRECTS;
#endif
#ifdef IPCTL_SOURCEROUTE
    case 91: return IPCTL_SOURCEROUTE;
#endif
#ifdef IPCTL_STATS
    case 92: return IPCTL_STATS;
#endif
#ifdef IPPORT_EPHEMERALFIRST
    case 93: return IPPORT_EPHEMERALFIRST;
#endif
#ifdef IPPORT_EPHEMERALLAST
    case 94: return IPPORT_EPHEMERALLAST;
#endif
#ifdef IPPORT_HIFIRSTAUTO
    case 95: return IPPORT_HIFIRSTAUTO;
#endif
#ifdef IPPORT_HILASTAUTO
    case 96: return IPPORT_HILASTAUTO;
#endif
#ifdef IPPORT_MAX
    case 97: return IPPORT_MAX;
#endif
#ifdef IPPORT_RESERVED
    case 98: return IPPORT_RESERVED;
#endif
#ifdef IPPORT_RESERVEDSTART
    case 99: return IPPORT_RESERVEDSTART;
#endif
#ifdef IPPORT_USERRESERVED
    case 100: return IPPORT_USERRESERVED;
#endif
#ifdef IPV6_2292DSTOPTS
    case 101: return IPV6_2292DSTOPTS;
#endif
#ifdef IPV6_2292HOPLIMIT
    case 102: return IPV6_2292HOPLIMIT;
#endif
#ifdef IPV6_2292HOPOPTS
    case 103: return IPV6_2292HOPOPTS;
#endif
#ifdef IPV6_2292PKTINFO
    case 104: return IPV6_2292PKTINFO;
#endif
#ifdef IPV6_2292PKTOPTIONS
    case 105: return IPV6_2292PKTOPTIONS;
#endif
#ifdef IPV6_2292RTHDR
    case 106: return IPV6_2292RTHDR;
#endif
#ifdef IPV6_ADDRFORM
    case 107: return IPV6_ADDRFORM;
#endif
#ifdef IPV6_ADDR_PREFERENCES
    case 108: return IPV6_ADDR_PREFERENCES;
#endif
#ifdef IPV6_ADD_MEMBERSHIP
    case 109: return IPV6_ADD_MEMBERSHIP;
#endif
#ifdef IPV6_AUTHHDR
    case 110: return IPV6_AUTHHDR;
#endif
#ifdef IPV6_AUTOFLOWLABEL
    case 111: return IPV6_AUTOFLOWLABEL;
#endif
#ifdef IPV6_CHECKSUM
    case 112: return IPV6_CHECKSUM;
#endif
#ifdef IPV6_DONTFRAG
    case 113: return IPV6_DONTFRAG;
#endif
#ifdef IPV6_DROP_MEMBERSHIP
    case 114: return IPV6_DROP_MEMBERSHIP;
#endif
#ifdef IPV6_DSTOPTS
    case 115: return IPV6_DSTOPTS;
#endif
#ifdef IPV6_FLOWINFO
    case 116: return IPV6_FLOWINFO;
#endif
#ifdef IPV6_FLOWINFO_FLOWLABEL
    case 117: return IPV6_FLOWINFO_FLOWLABEL;
#endif
#ifdef IPV6_FLOWINFO_PRIORITY
    case 118: return IPV6_FLOWINFO_PRIORITY;
#endif
#ifdef IPV6_FLOWINFO_SEND
    case 119: return IPV6_FLOWINFO_SEND;
#endif
#ifdef IPV6_FLOWLABEL_MGR
    case 120: return IPV6_FLOWLABEL_MGR;
#endif
#ifdef IPV6_FL_A_GET
    case 121: return IPV6_FL_A_GET;
#endif
#ifdef IPV6_FL_A_PUT
    case 122: return IPV6_FL_A_PUT;
#endif
#ifdef IPV6_FL_A_RENEW
    case 123: return IPV6_FL_A_RENEW;
#endif
#ifdef IPV6_FL_F_CREATE
    case 124: return IPV6_FL_F_CREATE;
#endif
#ifdef IPV6_FL_F_EXCL
    case 125: return IPV6_FL_F_EXCL;
#endif
#ifdef IPV6_FL_F_REFLECT
    case 126: return IPV6_FL_F_REFLECT;
#endif
#ifdef IPV6_FL_F_REMOTE
    case 127: return IPV6_FL_F_REMOTE;
#endif
#ifdef IPV6_FL_S_ANY
    case 128: return IPV6_FL_S_ANY;
#endif
#ifdef IPV6_FL_S_EXCL
    case 129: return IPV6_FL_S_EXCL;
#endif
#ifdef IPV6_FL_S_NONE
    case 130: return IPV6_FL_S_NONE;
#endif
#ifdef IPV6_FL_S_PROCESS
    case 131: return IPV6_FL_S_PROCESS;
#endif
#ifdef IPV6_FL_S_USER
    case 132: return IPV6_FL_S_USER;
#endif
#ifdef IPV6_HOPLIMIT
    case 133: return IPV6_HOPLIMIT;
#endif
#ifdef IPV6_HOPOPTS
    case 134: return IPV6_HOPOPTS;
#endif
#ifdef IPV6_IPSEC_POLICY
    case 135: return IPV6_IPSEC_POLICY;
#endif
#ifdef IPV6_JOIN_ANYCAST
    case 136: return IPV6_JOIN_ANYCAST;
#endif
#ifdef IPV6_LEAVE_ANYCAST
    case 137: return IPV6_LEAVE_ANYCAST;
#endif
#ifdef IPV6_MINHOPCOUNT
    case 138: return IPV6_MINHOPCOUNT;
#endif
#ifdef IPV6_MTU
    case 139: return IPV6_MTU;
#endif
#ifdef IPV6_MTU_DISCOVER
    case 140: return IPV6_MTU_DISCOVER;
#endif
#ifdef IPV6_MULTICAST_HOPS
    case 141: return IPV6_MULTICAST_HOPS;
#endif
#ifdef IPV6_MULTICAST_IF
    case 142: return IPV6_MULTICAST_IF;
#endif
#ifdef IPV6_MULTICAST_LOOP
    case 143: return IPV6_MULTICAST_LOOP;
#endif
#ifdef IPV6_NEXTHOP
    case 144: return IPV6_NEXTHOP;
#endif
#ifdef IPV6_ORIGDSTADDR
    case 145: return IPV6_ORIGDSTADDR;
#endif
#ifdef IPV6_PATHMTU
    case 146: return IPV6_PATHMTU;
#endif
#ifdef IPV6_PKTINFO
    case 147: return IPV6_PKTINFO;
#endif
#ifdef IPV6_PMTUDISC_DO
    case 148: return IPV6_PMTUDISC_DO;
#endif
#ifdef IPV6_PMTUDISC_DONT
    case 149: return IPV6_PMTUDISC_DONT;
#endif
#ifdef IPV6_PMTUDISC_INTERFACE
    case 150: return IPV6_PMTUDISC_INTERFACE;
#endif
#ifdef IPV6_PMTUDISC_OMIT
    case 151: return IPV6_PMTUDISC_OMIT;
#endif
#ifdef IPV6_PMTUDISC_PROBE
    case 152: return IPV6_PMTUDISC_PROBE;
#endif
#ifdef IPV6_PMTUDISC_WANT
    case 153: return IPV6_PMTUDISC_WANT;
#endif
#ifdef IPV6_PREFER_SRC_CGA
    case 154: return IPV6_PREFER_SRC_CGA;
#endif
#ifdef IPV6_PREFER_SRC_COA
    case 155: return IPV6_PREFER_SRC_COA;
#endif
#ifdef IPV6_PREFER_SRC_HOME
    case 156: return IPV6_PREFER_SRC_HOME;
#endif
#ifdef IPV6_PREFER_SRC_NONCGA
    case 157: return IPV6_PREFER_SRC_NONCGA;
#endif
#ifdef IPV6_PREFER_SRC_PUBLIC
    case 158: return IPV6_PREFER_SRC_PUBLIC;
#endif
#ifdef IPV6_PREFER_SRC_PUBTMP_DEFAULT
    case 159: return IPV6_PREFER_SRC_PUBTMP_DEFAULT;
#endif
#ifdef IPV6_PREFER_SRC_TMP
    case 160: return IPV6_PREFER_SRC_TMP;
#endif
#ifdef IPV6_PRIORITY_10
    case 161: return IPV6_PRIORITY_10;
#endif
#ifdef IPV6_PRIORITY_11
    case 162: return IPV6_PRIORITY_11;
#endif
#ifdef IPV6_PRIORITY_12
    case 163: return IPV6_PRIORITY_12;
#endif
#ifdef IPV6_PRIORITY_13
    case 164: return IPV6_PRIORITY_13;
#endif
#ifdef IPV6_PRIORITY_14
    case 165: return IPV6_PRIORITY_14;
#endif
#ifdef IPV6_PRIORITY_15
    case 166: return IPV6_PRIORITY_15;
#endif
#ifdef IPV6_PRIORITY_8
    case 167: return IPV6_PRIORITY_8;
#endif
#ifdef IPV6_PRIORITY_9
    case 168: return IPV6_PRIORITY_9;
#endif
#ifdef IPV6_PRIORITY_BULK
    case 169: return IPV6_PRIORITY_BULK;
#endif
#ifdef IPV6_PRIORITY_CONTROL
    case 170: return IPV6_PRIORITY_CONTROL;
#endif
#ifdef IPV6_PRIORITY_FILLER
    case 171: return IPV6_PRIORITY_FILLER;
#endif
#ifdef IPV6_PRIORITY_INTERACTIVE
    case 172: return IPV6_PRIORITY_INTERACTIVE;
#endif
#ifdef IPV6_PRIORITY_RESERVED1
    case 173: return IPV6_PRIORITY_RESERVED1;
#endif
#ifdef IPV6_PRIORITY_RESERVED2
    case 174: return IPV6_PRIORITY_RESERVED2;
#endif
#ifdef IPV6_PRIORITY_UNATTENDED
    case 175: return IPV6_PRIORITY_UNATTENDED;
#endif
#ifdef IPV6_PRIORITY_UNCHARACTERIZED
    case 176: return IPV6_PRIORITY_UNCHARACTERIZED;
#endif
#ifdef IPV6_RECVDSTOPTS
    case 177: return IPV6_RECVDSTOPTS;
#endif
#ifdef IPV6_RECVERR
    case 178: return IPV6_RECVERR;
#endif
#ifdef IPV6_RECVHOPLIMIT
    case 179: return IPV6_RECVHOPLIMIT;
#endif
#ifdef IPV6_RECVHOPOPTS
    case 180: return IPV6_RECVHOPOPTS;
#endif
#ifdef IPV6_RECVORIGDSTADDR
    case 181: return IPV6_RECVORIGDSTADDR;
#endif
#ifdef IPV6_RECVPATHMTU
    case 182: return IPV6_RECVPATHMTU;
#endif
#ifdef IPV6_RECVPKTINFO
    case 183: return IPV6_RECVPKTINFO;
#endif
#ifdef IPV6_RECVRTHDR
    case 184: return IPV6_RECVRTHDR;
#endif
#ifdef IPV6_RECVTCLASS
    case 185: return IPV6_RECVTCLASS;
#endif
#ifdef IPV6_ROUTER_ALERT
    case 186: return IPV6_ROUTER_ALERT;
#endif
#ifdef IPV6_RTHDR
    case 187: return IPV6_RTHDR;
#endif
#ifdef IPV6_RTHDRDSTOPTS
    case 188: return IPV6_RTHDRDSTOPTS;
#endif
#ifdef IPV6_TCLASS
    case 189: return IPV6_TCLASS;
#endif
#ifdef IPV6_TLV_HAO
    case 190: return IPV6_TLV_HAO;
#endif
#ifdef IPV6_TLV_JUMBO
    case 191: return IPV6_TLV_JUMBO;
#endif
#ifdef IPV6_TLV_PAD1
    case 192: return IPV6_TLV_PAD1;
#endif
#ifdef IPV6_TLV_PADN
    case 193: return IPV6_TLV_PADN;
#endif
#ifdef IPV6_TLV_ROUTERALERT
    case 194: return IPV6_TLV_ROUTERALERT;
#endif
#ifdef IPV6_TRANSPARENT
    case 195: return IPV6_TRANSPARENT;
#endif
#ifdef IPV6_UNICAST_HOPS
    case 196: return IPV6_UNICAST_HOPS;
#endif
#ifdef IPV6_UNICAST_IF
    case 197: return IPV6_UNICAST_IF;
#endif
#ifdef IPV6_USE_MIN_MTU
    case 198: return IPV6_USE_MIN_MTU;
#endif
#ifdef IPV6_V6ONLY
    case 199: return IPV6_V6ONLY;
#endif
#ifdef IPV6_XFRM_POLICY
    case 200: return IPV6_XFRM_POLICY;
#endif
#ifdef IPX_ADDRESS
    case 201: return IPX_ADDRESS;
#endif
#ifdef IPX_ADDRESS_NOTIFY
    case 202: return IPX_ADDRESS_NOTIFY;
#endif
#ifdef IPX_CRTITF
    case 203: return IPX_CRTITF;
#endif
#ifdef IPX_DLTITF
    case 204: return IPX_DLTITF;
#endif
#ifdef IPX_DSTYPE
    case 205: return IPX_DSTYPE;
#endif
#ifdef IPX_EXTENDED_ADDRESS
    case 206: return IPX_EXTENDED_ADDRESS;
#endif
#ifdef IPX_FILTERPTYPE
    case 207: return IPX_FILTERPTYPE;
#endif
#ifdef IPX_FRAME_8022
    case 208: return IPX_FRAME_8022;
#endif
#ifdef IPX_FRAME_8023
    case 209: return IPX_FRAME_8023;
#endif
#ifdef IPX_FRAME_ETHERII
    case 210: return IPX_FRAME_ETHERII;
#endif
#ifdef IPX_FRAME_NONE
    case 211: return IPX_FRAME_NONE;
#endif
#ifdef IPX_FRAME_SNAP
    case 212: return IPX_FRAME_SNAP;
#endif
#ifdef IPX_FRAME_TR_8022
    case 213: return IPX_FRAME_TR_8022;
#endif
#ifdef IPX_GETNETINFO
    case 214: return IPX_GETNETINFO;
#endif
#ifdef IPX_GETNETINFO_NORIP
    case 215: return IPX_GETNETINFO_NORIP;
#endif
#ifdef IPX_IMMEDIATESPXACK
    case 216: return IPX_IMMEDIATESPXACK;
#endif
#ifdef IPX_INTERNAL
    case 217: return IPX_INTERNAL;
#endif
#ifdef IPX_MAXSIZE
    case 218: return IPX_MAXSIZE;
#endif
#ifdef IPX_MAX_ADAPTER_NUM
    case 219: return IPX_MAX_ADAPTER_NUM;
#endif
#ifdef IPX_MTU
    case 220: return IPX_MTU;
#endif
#ifdef IPX_NODE_LEN
    case 221: return IPX_NODE_LEN;
#endif
#ifdef IPX_PRIMARY
    case 222: return IPX_PRIMARY;
#endif
#ifdef IPX_PTYPE
    case 223: return IPX_PTYPE;
#endif
#ifdef IPX_RECEIVE_BROADCAST
    case 224: return IPX_RECEIVE_BROADCAST;
#endif
#ifdef IPX_RECVHDR
    case 225: return IPX_RECVHDR;
#endif
#ifdef IPX_RERIPNETNUMBER
    case 226: return IPX_RERIPNETNUMBER;
#endif
#ifdef IPX_ROUTE_NO_ROUTER
    case 227: return IPX_ROUTE_NO_ROUTER;
#endif
#ifdef IPX_RT_8022
    case 228: return IPX_RT_8022;
#endif
#ifdef IPX_RT_BLUEBOOK
    case 229: return IPX_RT_BLUEBOOK;
#endif
#ifdef IPX_RT_ROUTED
    case 230: return IPX_RT_ROUTED;
#endif
#ifdef IPX_RT_SNAP
    case 231: return IPX_RT_SNAP;
#endif
#ifdef IPX_SPECIAL_NONE
    case 232: return IPX_SPECIAL_NONE;
#endif
#ifdef IPX_SPXGETCONNECTIONSTATUS
    case 233: return IPX_SPXGETCONNECTIONSTATUS;
#endif
#ifdef IPX_STOPFILTERPTYPE
    case 234: return IPX_STOPFILTERPTYPE;
#endif
#ifdef IPX_TYPE
    case 235: return IPX_TYPE;
#endif
#ifdef IP_ADD_MEMBERSHIP
    case 236: return IP_ADD_MEMBERSHIP;
#endif
#ifdef IP_ADD_SOURCE_MEMBERSHIP
    case 237: return IP_ADD_SOURCE_MEMBERSHIP;
#endif
#ifdef IP_BINDANY
    case 238: return IP_BINDANY;
#endif
#ifdef IP_BINDMULTI
    case 239: return IP_BINDMULTI;
#endif
#ifdef IP_BIND_ADDRESS_NO_PORT
    case 240: return IP_BIND_ADDRESS_NO_PORT;
#endif
#ifdef IP_BLOCK_SOURCE
    case 241: return IP_BLOCK_SOURCE;
#endif
#ifdef IP_BOUND_IF
    case 242: return IP_BOUND_IF;
#endif
#ifdef IP_CHECKSUM
    case 243: return IP_CHECKSUM;
#endif
#ifdef IP_DEFAULT_MULTICAST_LOOP
    case 244: return IP_DEFAULT_MULTICAST_LOOP;
#endif
#ifdef IP_DEFAULT_MULTICAST_TTL
    case 245: return IP_DEFAULT_MULTICAST_TTL;
#endif
#ifdef IP_DONTFRAG
    case 246: return IP_DONTFRAG;
#endif
#ifdef IP_DROP_MEMBERSHIP
    case 247: return IP_DROP_MEMBERSHIP;
#endif
#ifdef IP_DROP_SOURCE_MEMBERSHIP
    case 248: return IP_DROP_SOURCE_MEMBERSHIP;
#endif
#ifdef IP_DUMMYNET3
    case 249: return IP_DUMMYNET3;
#endif
#ifdef IP_DUMMYNET_CONFIGURE
    case 250: return IP_DUMMYNET_CONFIGURE;
#endif
#ifdef IP_DUMMYNET_DEL
    case 251: return IP_DUMMYNET_DEL;
#endif
#ifdef IP_DUMMYNET_FLUSH
    case 252: return IP_DUMMYNET_FLUSH;
#endif
#ifdef IP_DUMMYNET_GET
    case 253: return IP_DUMMYNET_GET;
#endif
#ifdef IP_FAITH
    case 254: return IP_FAITH;
#endif
#ifdef IP_FLOWID
    case 255: return IP_FLOWID;
#endif
#ifdef IP_FLOWTYPE
    case 256: return IP_FLOWTYPE;
#endif
#ifdef IP_FREEBIND
    case 257: return IP_FREEBIND;
#endif
#ifdef IP_FW3
    case 258: return IP_FW3;
#endif
#ifdef IP_FW_ADD
    case 259: return IP_FW_ADD;
#endif
#ifdef IP_FW_DEL
    case 260: return IP_FW_DEL;
#endif
#ifdef IP_FW_FLUSH
    case 261: return IP_FW_FLUSH;
#endif
#ifdef IP_FW_GET
    case 262: return IP_FW_GET;
#endif
#ifdef IP_FW_NAT_CFG
    case 263: return IP_FW_NAT_CFG;
#endif
#ifdef IP_FW_NAT_DEL
    case 264: return IP_FW_NAT_DEL;
#endif
#ifdef IP_FW_NAT_GET_CONFIG
    case 265: return IP_FW_NAT_GET_CONFIG;
#endif
#ifdef IP_FW_NAT_GET_LOG
    case 266: return IP_FW_NAT_GET_LOG;
#endif
#ifdef IP_FW_RESETLOG
    case 267: return IP_FW_RESETLOG;
#endif
#ifdef IP_FW_TABLE_ADD
    case 268: return IP_FW_TABLE_ADD;
#endif
#ifdef IP_FW_TABLE_DEL
    case 269: return IP_FW_TABLE_DEL;
#endif
#ifdef IP_FW_TABLE_FLUSH
    case 270: return IP_FW_TABLE_FLUSH;
#endif
#ifdef IP_FW_TABLE_GETSIZE
    case 271: return IP_FW_TABLE_GETSIZE;
#endif
#ifdef IP_FW_TABLE_LIST
    case 272: return IP_FW_TABLE_LIST;
#endif
#ifdef IP_FW_ZERO
    case 273: return IP_FW_ZERO;
#endif
#ifdef IP_HDRINCL
    case 274: return IP_HDRINCL;
#endif
#ifdef IP_IPSEC_POLICY
    case 275: return IP_IPSEC_POLICY;
#endif
#ifdef IP_MAX_GROUP_SRC_FILTER
    case 276: return IP_MAX_GROUP_SRC_FILTER;
#endif
#ifdef IP_MAX_MEMBERSHIPS
    case 277: return IP_MAX_MEMBERSHIPS;
#endif
#ifdef IP_MAX_SOCK_MUTE_FILTER
    case 278: return IP_MAX_SOCK_MUTE_FILTER;
#endif
#ifdef IP_MAX_SOCK_SRC_FILTER
    case 279: return IP_MAX_SOCK_SRC_FILTER;
#endif
#ifdef IP_MAX_SOURCE_FILTER
    case 280: return IP_MAX_SOURCE_FILTER;
#endif
#ifdef IP_MINTTL
    case 281: return IP_MINTTL;
#endif
#ifdef IP_MIN_MEMBERSHIPS
    case 282: return IP_MIN_MEMBERSHIPS;
#endif
#ifdef IP_MSFILTER
    case 283: return IP_MSFILTER;
#endif
#ifdef IP_MTU
    case 284: return IP_MTU;
#endif
#ifdef IP_MTU_DISCOVER
    case 285: return IP_MTU_DISCOVER;
#endif
#ifdef IP_MULTICAST_ALL
    case 286: return IP_MULTICAST_ALL;
#endif
#ifdef IP_MULTICAST_IF
    case 287: return IP_MULTICAST_IF;
#endif
#ifdef IP_MULTICAST_IFINDEX
    case 288: return IP_MULTICAST_IFINDEX;
#endif
#ifdef IP_MULTICAST_LOOP
    case 289: return IP_MULTICAST_LOOP;
#endif
#ifdef IP_MULTICAST_TTL
    case 290: return IP_MULTICAST_TTL;
#endif
#ifdef IP_MULTICAST_VIF
    case 291: return IP_MULTICAST_VIF;
#endif
#ifdef IP_NAT_XXX
    case 292: return IP_NAT_XXX;
#endif
#ifdef IP_NODEFRAG
    case 293: return IP_NODEFRAG;
#endif
#ifdef IP_OLD_FW_ADD
    case 294: return IP_OLD_FW_ADD;
#endif
#ifdef IP_OLD_FW_DEL
    case 295: return IP_OLD_FW_DEL;
#endif
#ifdef IP_OLD_FW_FLUSH
    case 296: return IP_OLD_FW_FLUSH;
#endif
#ifdef IP_OLD_FW_GET
    case 297: return IP_OLD_FW_GET;
#endif
#ifdef IP_OLD_FW_RESETLOG
    case 298: return IP_OLD_FW_RESETLOG;
#endif
#ifdef IP_OLD_FW_ZERO
    case 299: return IP_OLD_FW_ZERO;
#endif
#ifdef IP_ONESBCAST
    case 300: return IP_ONESBCAST;
#endif
#ifdef IP_OPTIONS
    case 301: return IP_OPTIONS;
#endif
#ifdef IP_ORIGDSTADDR
    case 302: return IP_ORIGDSTADDR;
#endif
#ifdef IP_PASSSEC
    case 303: return IP_PASSSEC;
#endif
#ifdef IP_PKTINFO
    case 304: return IP_PKTINFO;
#endif
#ifdef IP_PKTOPTIONS
    case 305: return IP_PKTOPTIONS;
#endif
#ifdef IP_PMTUDISC_DO
    case 306: return IP_PMTUDISC_DO;
#endif
#ifdef IP_PMTUDISC_DONT
    case 307: return IP_PMTUDISC_DONT;
#endif
#ifdef IP_PMTUDISC_INTERFACE
    case 308: return IP_PMTUDISC_INTERFACE;
#endif
#ifdef IP_PMTUDISC_OMIT
    case 309: return IP_PMTUDISC_OMIT;
#endif
#ifdef IP_PMTUDISC_PROBE
    case 310: return IP_PMTUDISC_PROBE;
#endif
#ifdef IP_PMTUDISC_WANT
    case 311: return IP_PMTUDISC_WANT;
#endif
#ifdef IP_PORTRANGE
    case 312: return IP_PORTRANGE;
#endif
#ifdef IP_PORTRANGE_DEFAULT
    case 313: return IP_PORTRANGE_DEFAULT;
#endif
#ifdef IP_PORTRANGE_HIGH
    case 314: return IP_PORTRANGE_HIGH;
#endif
#ifdef IP_PORTRANGE_LOW
    case 315: return IP_PORTRANGE_LOW;
#endif
#ifdef IP_RECVDSTADDR
    case 316: return IP_RECVDSTADDR;
#endif
#ifdef IP_RECVERR
    case 317: return IP_RECVERR;
#endif
#ifdef IP_RECVFLOWID
    case 318: return IP_RECVFLOWID;
#endif
#ifdef IP_RECVIF
    case 319: return IP_RECVIF;
#endif
#ifdef IP_RECVOPTS
    case 320: return IP_RECVOPTS;
#endif
#ifdef IP_RECVORIGDSTADDR
    case 321: return IP_RECVORIGDSTADDR;
#endif
#ifdef IP_RECVPKTINFO
    case 322: return IP_RECVPKTINFO;
#endif
#ifdef IP_RECVRETOPTS
    case 323: return IP_RECVRETOPTS;
#endif
#ifdef IP_RECVRSSBUCKETID
    case 324: return IP_RECVRSSBUCKETID;
#endif
#ifdef IP_RECVTOS
    case 325: return IP_RECVTOS;
#endif
#ifdef IP_RECVTTL
    case 326: return IP_RECVTTL;
#endif
#ifdef IP_RETOPTS
    case 327: return IP_RETOPTS;
#endif
#ifdef IP_ROUTER_ALERT
    case 328: return IP_ROUTER_ALERT;
#endif
#ifdef IP_RSSBUCKETID
    case 329: return IP_RSSBUCKETID;
#endif
#ifdef IP_RSS_LISTEN_BUCKET
    case 330: return IP_RSS_LISTEN_BUCKET;
#endif
#ifdef IP_RSVP_OFF
    case 331: return IP_RSVP_OFF;
#endif
#ifdef IP_RSVP_ON
    case 332: return IP_RSVP_ON;
#endif
#ifdef IP_RSVP_VIF_OFF
    case 333: return IP_RSVP_VIF_OFF;
#endif
#ifdef IP_RSVP_VIF_ON
    case 334: return IP_RSVP_VIF_ON;
#endif
#ifdef IP_SENDSRCADDR
    case 335: return IP_SENDSRCADDR;
#endif
#ifdef IP_STRIPHDR
    case 336: return IP_STRIPHDR;
#endif
#ifdef IP_TOS
    case 337: return IP_TOS;
#endif
#ifdef IP_TRAFFIC_MGT_BACKGROUND
    case 338: return IP_TRAFFIC_MGT_BACKGROUND;
#endif
#ifdef IP_TRANSPARENT
    case 339: return IP_TRANSPARENT;
#endif
#ifdef IP_TTL
    case 340: return IP_TTL;
#endif
#ifdef IP_UNBLOCK_SOURCE
    case 341: return IP_UNBLOCK_SOURCE;
#endif
#ifdef IP_UNICAST_IF
    case 342: return IP_UNICAST_IF;
#endif
#ifdef IP_XFRM_POLICY
    case 343: return IP_XFRM_POLICY;
#endif
#ifdef LOCAL_CONNWAIT
    case 344: return LOCAL_CONNWAIT;
#endif
#ifdef LOCAL_CREDS
    case 345: return LOCAL_CREDS;
#endif
#ifdef LOCAL_PEERCRED
    case 346: return LOCAL_PEERCRED;
#endif
#ifdef LOCAL_PEEREPID
    case 347: return LOCAL_PEEREPID;
#endif
#ifdef LOCAL_PEEREUUID
    case 348: return LOCAL_PEEREUUID;
#endif
#ifdef LOCAL_PEERPID
    case 349: return LOCAL_PEERPID;
#endif
#ifdef LOCAL_PEERUUID
    case 350: return LOCAL_PEERUUID;
#endif
#ifdef LOCAL_VENDOR
    case 351: return LOCAL_VENDOR;
#endif
#ifdef MAX_TCPOPTLEN
    case 352: return MAX_TCPOPTLEN;
#endif
#ifdef MCAST_BLOCK_SOURCE
    case 353: return MCAST_BLOCK_SOURCE;
#endif
#ifdef MCAST_EXCLUDE
    case 354: return MCAST_EXCLUDE;
#endif
#ifdef MCAST_INCLUDE
    case 355: return MCAST_INCLUDE;
#endif
#ifdef MCAST_JOIN_GROUP
    case 356: return MCAST_JOIN_GROUP;
#endif
#ifdef MCAST_JOIN_SOURCE_GROUP
    case 357: return MCAST_JOIN_SOURCE_GROUP;
#endif
#ifdef MCAST_LEAVE_GROUP
    case 358: return MCAST_LEAVE_GROUP;
#endif
#ifdef MCAST_LEAVE_SOURCE_GROUP
    case 359: return MCAST_LEAVE_SOURCE_GROUP;
#endif
#ifdef MCAST_MSFILTER
    case 360: return MCAST_MSFILTER;
#endif
#ifdef MCAST_UNBLOCK_SOURCE
    case 361: return MCAST_UNBLOCK_SOURCE;
#endif
#ifdef MCAST_UNDEFINED
    case 362: return MCAST_UNDEFINED;
#endif
#ifdef MRT_ADD_BW_UPCALL
    case 363: return MRT_ADD_BW_UPCALL;
#endif
#ifdef MRT_ADD_MFC
    case 364: return MRT_ADD_MFC;
#endif
#ifdef MRT_ADD_VIF
    case 365: return MRT_ADD_VIF;
#endif
#ifdef MRT_API_CONFIG
    case 366: return MRT_API_CONFIG;
#endif
#ifdef MRT_API_FLAGS_ALL
    case 367: return MRT_API_FLAGS_ALL;
#endif
#ifdef MRT_API_SUPPORT
    case 368: return MRT_API_SUPPORT;
#endif
#ifdef MRT_ASSERT
    case 369: return MRT_ASSERT;
#endif
#ifdef MRT_DEL_BW_UPCALL
    case 370: return MRT_DEL_BW_UPCALL;
#endif
#ifdef MRT_DEL_MFC
    case 371: return MRT_DEL_MFC;
#endif
#ifdef MRT_DEL_VIF
    case 372: return MRT_DEL_VIF;
#endif
#ifdef MRT_DONE
    case 373: return MRT_DONE;
#endif
#ifdef MRT_INIT
    case 374: return MRT_INIT;
#endif
#ifdef MRT_MFC_BW_UPCALL
    case 375: return MRT_MFC_BW_UPCALL;
#endif
#ifdef MRT_MFC_FLAGS_ALL
    case 376: return MRT_MFC_FLAGS_ALL;
#endif
#ifdef MRT_MFC_FLAGS_BORDER_VIF
    case 377: return MRT_MFC_FLAGS_BORDER_VIF;
#endif
#ifdef MRT_MFC_FLAGS_DISABLE_WRONGVIF
    case 378: return MRT_MFC_FLAGS_DISABLE_WRONGVIF;
#endif
#ifdef MRT_MFC_RP
    case 379: return MRT_MFC_RP;
#endif
#ifdef MRT_PIM
    case 380: return MRT_PIM;
#endif
#ifdef MRT_VERSION
    case 381: return MRT_VERSION;
#endif
#ifdef MSG_NOTIFICATION
    case 382: return MSG_NOTIFICATION;
#endif
#ifdef MSG_SOCALLBCK
    case 383: return MSG_SOCALLBCK;
#endif
#ifdef NDRVPROTO_NDRV
    case 384: return NDRVPROTO_NDRV;
#endif
#ifdef NDRV_ADDMULTICAST
    case 385: return NDRV_ADDMULTICAST;
#endif
#ifdef NDRV_DELDMXSPEC
    case 386: return NDRV_DELDMXSPEC;
#endif
#ifdef NDRV_DELMULTICAST
    case 387: return NDRV_DELMULTICAST;
#endif
#ifdef NDRV_DEMUXTYPE_ETHERTYPE
    case 388: return NDRV_DEMUXTYPE_ETHERTYPE;
#endif
#ifdef NDRV_DEMUXTYPE_SAP
    case 389: return NDRV_DEMUXTYPE_SAP;
#endif
#ifdef NDRV_DEMUXTYPE_SNAP
    case 390: return NDRV_DEMUXTYPE_SNAP;
#endif
#ifdef NDRV_DMUX_MAX_DESCR
    case 391: return NDRV_DMUX_MAX_DESCR;
#endif
#ifdef NDRV_PROTOCOL_DESC_VERS
    case 392: return NDRV_PROTOCOL_DESC_VERS;
#endif
#ifdef NDRV_SETDMXSPEC
    case 393: return NDRV_SETDMXSPEC;
#endif
#ifdef NETLINK_ADD_MEMBERSHIP
    case 394: return NETLINK_ADD_MEMBERSHIP;
#endif
#ifdef NETLINK_AUDIT
    case 395: return NETLINK_AUDIT;
#endif
#ifdef NETLINK_BROADCAST_ERROR
    case 396: return NETLINK_BROADCAST_ERROR;
#endif
#ifdef NETLINK_CAP_ACK
    case 397: return NETLINK_CAP_ACK;
#endif
#ifdef NETLINK_CONNECTOR
    case 398: return NETLINK_CONNECTOR;
#endif
#ifdef NETLINK_CRYPTO
    case 399: return NETLINK_CRYPTO;
#endif
#ifdef NETLINK_DNRTMSG
    case 400: return NETLINK_DNRTMSG;
#endif
#ifdef NETLINK_DROP_MEMBERSHIP
    case 401: return NETLINK_DROP_MEMBERSHIP;
#endif
#ifdef NETLINK_ECRYPTFS
    case 402: return NETLINK_ECRYPTFS;
#endif
#ifdef NETLINK_FIB_LOOKUP
    case 403: return NETLINK_FIB_LOOKUP;
#endif
#ifdef NETLINK_FIREWALL
    case 404: return NETLINK_FIREWALL;
#endif
#ifdef NETLINK_GENERIC
    case 405: return NETLINK_GENERIC;
#endif
#ifdef NETLINK_INET_DIAG
    case 406: return NETLINK_INET_DIAG;
#endif
#ifdef NETLINK_IP6_FW
    case 407: return NETLINK_IP6_FW;
#endif
#ifdef NETLINK_ISCSI
    case 408: return NETLINK_ISCSI;
#endif
#ifdef NETLINK_KOBJECT_UEVENT
    case 409: return NETLINK_KOBJECT_UEVENT;
#endif
#ifdef NETLINK_LISTEN_ALL_NSID
    case 410: return NETLINK_LISTEN_ALL_NSID;
#endif
#ifdef NETLINK_LIST_MEMBERSHIPS
    case 411: return NETLINK_LIST_MEMBERSHIPS;
#endif
#ifdef NETLINK_NETFILTER
    case 412: return NETLINK_NETFILTER;
#endif
#ifdef NETLINK_NFLOG
    case 413: return NETLINK_NFLOG;
#endif
#ifdef NETLINK_NO_ENOBUFS
    case 414: return NETLINK_NO_ENOBUFS;
#endif
#ifdef NETLINK_PKTINFO
    case 415: return NETLINK_PKTINFO;
#endif
#ifdef NETLINK_RDMA
    case 416: return NETLINK_RDMA;
#endif
#ifdef NETLINK_ROUTE
    case 417: return NETLINK_ROUTE;
#endif
#ifdef NETLINK_RX_RING
    case 418: return NETLINK_RX_RING;
#endif
#ifdef NETLINK_SCSITRANSPORT
    case 419: return NETLINK_SCSITRANSPORT;
#endif
#ifdef NETLINK_SELINUX
    case 420: return NETLINK_SELINUX;
#endif
#ifdef NETLINK_SOCK_DIAG
    case 421: return NETLINK_SOCK_DIAG;
#endif
#ifdef NETLINK_TX_RING
    case 422: return NETLINK_TX_RING;
#endif
#ifdef NETLINK_UNUSED
    case 423: return NETLINK_UNUSED;
#endif
#ifdef NETLINK_USERSOCK
    case 424: return NETLINK_USERSOCK;
#endif
#ifdef NETLINK_XFRM
    case 425: return NETLINK_XFRM;
#endif
#ifdef NETROM_IDLE
    case 426: return NETROM_IDLE;
#endif
#ifdef NETROM_KILL
    case 427: return NETROM_KILL;
#endif
#ifdef NETROM_N2
    case 428: return NETROM_N2;
#endif
#ifdef NETROM_NEIGH
    case 429: return NETROM_NEIGH;
#endif
#ifdef NETROM_NODE
    case 430: return NETROM_NODE;
#endif
#ifdef NETROM_PACLEN
    case 431: return NETROM_PACLEN;
#endif
#ifdef NETROM_T1
    case 432: return NETROM_T1;
#endif
#ifdef NETROM_T2
    case 433: return NETROM_T2;
#endif
#ifdef NETROM_T4
    case 434: return NETROM_T4;
#endif
#ifdef NRDV_MULTICAST_ADDRS_PER_SOCK
    case 435: return NRDV_MULTICAST_ADDRS_PER_SOCK;
#endif
#ifdef PVD_CONFIG
    case 436: return PVD_CONFIG;
#endif
#ifdef RDS_CANCEL_SENT_TO
    case 437: return RDS_CANCEL_SENT_TO;
#endif
#ifdef RDS_CMSG_ATOMIC_CSWP
    case 438: return RDS_CMSG_ATOMIC_CSWP;
#endif
#ifdef RDS_CMSG_ATOMIC_FADD
    case 439: return RDS_CMSG_ATOMIC_FADD;
#endif
#ifdef RDS_CMSG_CONG_UPDATE
    case 440: return RDS_CMSG_CONG_UPDATE;
#endif
#ifdef RDS_CMSG_MASKED_ATOMIC_CSWP
    case 441: return RDS_CMSG_MASKED_ATOMIC_CSWP;
#endif
#ifdef RDS_CMSG_MASKED_ATOMIC_FADD
    case 442: return RDS_CMSG_MASKED_ATOMIC_FADD;
#endif
#ifdef RDS_CMSG_RDMA_ARGS
    case 443: return RDS_CMSG_RDMA_ARGS;
#endif
#ifdef RDS_CMSG_RDMA_DEST
    case 444: return RDS_CMSG_RDMA_DEST;
#endif
#ifdef RDS_CMSG_RDMA_MAP
    case 445: return RDS_CMSG_RDMA_MAP;
#endif
#ifdef RDS_CMSG_RDMA_STATUS
    case 446: return RDS_CMSG_RDMA_STATUS;
#endif
#ifdef RDS_CONG_MONITOR
    case 447: return RDS_CONG_MONITOR;
#endif
#ifdef RDS_CONG_MONITOR_SIZE
    case 448: return RDS_CONG_MONITOR_SIZE;
#endif
#ifdef RDS_FREE_MR
    case 449: return RDS_FREE_MR;
#endif
#ifdef RDS_GET_MR
    case 450: return RDS_GET_MR;
#endif
#ifdef RDS_GET_MR_FOR_DEST
    case 451: return RDS_GET_MR_FOR_DEST;
#endif
#ifdef RDS_IB_ABI_VERSION
    case 452: return RDS_IB_ABI_VERSION;
#endif
#ifdef RDS_IB_GID_LEN
    case 453: return RDS_IB_GID_LEN;
#endif
#ifdef RDS_INFO_CONNECTIONS
    case 454: return RDS_INFO_CONNECTIONS;
#endif
#ifdef RDS_INFO_CONNECTION_FLAG_CONNECTED
    case 455: return RDS_INFO_CONNECTION_FLAG_CONNECTED;
#endif
#ifdef RDS_INFO_CONNECTION_FLAG_CONNECTING
    case 456: return RDS_INFO_CONNECTION_FLAG_CONNECTING;
#endif
#ifdef RDS_INFO_CONNECTION_FLAG_SENDING
    case 457: return RDS_INFO_CONNECTION_FLAG_SENDING;
#endif
#ifdef RDS_INFO_CONNECTION_STATS
    case 458: return RDS_INFO_CONNECTION_STATS;
#endif
#ifdef RDS_INFO_COUNTERS
    case 459: return RDS_INFO_COUNTERS;
#endif
#ifdef RDS_INFO_FIRST
    case 460: return RDS_INFO_FIRST;
#endif
#ifdef RDS_INFO_IB_CONNECTIONS
    case 461: return RDS_INFO_IB_CONNECTIONS;
#endif
#ifdef RDS_INFO_IWARP_CONNECTIONS
    case 462: return RDS_INFO_IWARP_CONNECTIONS;
#endif
#ifdef RDS_INFO_LAST
    case 463: return RDS_INFO_LAST;
#endif
#ifdef RDS_INFO_MESSAGE_FLAG_ACK
    case 464: return RDS_INFO_MESSAGE_FLAG_ACK;
#endif
#ifdef RDS_INFO_MESSAGE_FLAG_FAST_ACK
    case 465: return RDS_INFO_MESSAGE_FLAG_FAST_ACK;
#endif
#ifdef RDS_INFO_RECV_MESSAGES
    case 466: return RDS_INFO_RECV_MESSAGES;
#endif
#ifdef RDS_INFO_RETRANS_MESSAGES
    case 467: return RDS_INFO_RETRANS_MESSAGES;
#endif
#ifdef RDS_INFO_SEND_MESSAGES
    case 468: return RDS_INFO_SEND_MESSAGES;
#endif
#ifdef RDS_INFO_SOCKETS
    case 469: return RDS_INFO_SOCKETS;
#endif
#ifdef RDS_INFO_TCP_SOCKETS
    case 470: return RDS_INFO_TCP_SOCKETS;
#endif
#ifdef RDS_RDMA_CANCELED
    case 471: return RDS_RDMA_CANCELED;
#endif
#ifdef RDS_RDMA_DONTWAIT
    case 472: return RDS_RDMA_DONTWAIT;
#endif
#ifdef RDS_RDMA_DROPPED
    case 473: return RDS_RDMA_DROPPED;
#endif
#ifdef RDS_RDMA_FENCE
    case 474: return RDS_RDMA_FENCE;
#endif
#ifdef RDS_RDMA_INVALIDATE
    case 475: return RDS_RDMA_INVALIDATE;
#endif
#ifdef RDS_RDMA_NOTIFY_ME
    case 476: return RDS_RDMA_NOTIFY_ME;
#endif
#ifdef RDS_RDMA_OTHER_ERROR
    case 477: return RDS_RDMA_OTHER_ERROR;
#endif
#ifdef RDS_RDMA_READWRITE
    case 478: return RDS_RDMA_READWRITE;
#endif
#ifdef RDS_RDMA_REMOTE_ERROR
    case 479: return RDS_RDMA_REMOTE_ERROR;
#endif
#ifdef RDS_RDMA_SILENT
    case 480: return RDS_RDMA_SILENT;
#endif
#ifdef RDS_RDMA_SUCCESS
    case 481: return RDS_RDMA_SUCCESS;
#endif
#ifdef RDS_RDMA_USE_ONCE
    case 482: return RDS_RDMA_USE_ONCE;
#endif
#ifdef RDS_RECVERR
    case 483: return RDS_RECVERR;
#endif
#ifdef RDS_TRANS_COUNT
    case 484: return RDS_TRANS_COUNT;
#endif
#ifdef RDS_TRANS_IB
    case 485: return RDS_TRANS_IB;
#endif
#ifdef RDS_TRANS_IWARP
    case 486: return RDS_TRANS_IWARP;
#endif
#ifdef RDS_TRANS_NONE
    case 487: return RDS_TRANS_NONE;
#endif
#ifdef RDS_TRANS_TCP
    case 488: return RDS_TRANS_TCP;
#endif
#ifdef ROSE_ACCESS_BARRED
    case 489: return ROSE_ACCESS_BARRED;
#endif
#ifdef ROSE_DEFER
    case 490: return ROSE_DEFER;
#endif
#ifdef ROSE_DTE_ORIGINATED
    case 491: return ROSE_DTE_ORIGINATED;
#endif
#ifdef ROSE_HOLDBACK
    case 492: return ROSE_HOLDBACK;
#endif
#ifdef ROSE_IDLE
    case 493: return ROSE_IDLE;
#endif
#ifdef ROSE_INVALID_FACILITY
    case 494: return ROSE_INVALID_FACILITY;
#endif
#ifdef ROSE_LOCAL_PROCEDURE
    case 495: return ROSE_LOCAL_PROCEDURE;
#endif
#ifdef ROSE_MAX_DIGIS
    case 496: return ROSE_MAX_DIGIS;
#endif
#ifdef ROSE_MTU
    case 497: return ROSE_MTU;
#endif
#ifdef ROSE_NETWORK_CONGESTION
    case 498: return ROSE_NETWORK_CONGESTION;
#endif
#ifdef ROSE_NOT_OBTAINABLE
    case 499: return ROSE_NOT_OBTAINABLE;
#endif
#ifdef ROSE_NUMBER_BUSY
    case 500: return ROSE_NUMBER_BUSY;
#endif
#ifdef ROSE_OUT_OF_ORDER
    case 501: return ROSE_OUT_OF_ORDER;
#endif
#ifdef ROSE_QBITINCL
    case 502: return ROSE_QBITINCL;
#endif
#ifdef ROSE_REMOTE_PROCEDURE
    case 503: return ROSE_REMOTE_PROCEDURE;
#endif
#ifdef ROSE_SHIP_ABSENT
    case 504: return ROSE_SHIP_ABSENT;
#endif
#ifdef ROSE_T1
    case 505: return ROSE_T1;
#endif
#ifdef ROSE_T2
    case 506: return ROSE_T2;
#endif
#ifdef ROSE_T3
    case 507: return ROSE_T3;
#endif
#ifdef SCM_HCI_RAW_DIRECTION
    case 508: return SCM_HCI_RAW_DIRECTION;
#endif
#ifdef SCM_TIMESTAMP
    case 509: return SCM_TIMESTAMP;
#endif
#ifdef SCM_TIMESTAMPING
    case 510: return SCM_TIMESTAMPING;
#endif
#ifdef SCM_TIMESTAMPNS
    case 511: return SCM_TIMESTAMPNS;
#endif
#ifdef SCM_WIFI_STATUS
    case 512: return SCM_WIFI_STATUS;
#endif
#ifdef SCTP_ABORT_ASSOCIATION
    case 513: return SCTP_ABORT_ASSOCIATION;
#endif
#ifdef SCTP_ADAPTATION_LAYER
    case 514: return SCTP_ADAPTATION_LAYER;
#endif
#ifdef SCTP_ADAPTION_LAYER
    case 515: return SCTP_ADAPTION_LAYER;
#endif
#ifdef SCTP_ADD_STREAMS
    case 516: return SCTP_ADD_STREAMS;
#endif
#ifdef SCTP_ADD_VRF_ID
    case 517: return SCTP_ADD_VRF_ID;
#endif
#ifdef SCTP_ASCONF
    case 518: return SCTP_ASCONF;
#endif
#ifdef SCTP_ASCONF_ACK
    case 519: return SCTP_ASCONF_ACK;
#endif
#ifdef SCTP_ASCONF_SUPPORTED
    case 520: return SCTP_ASCONF_SUPPORTED;
#endif
#ifdef SCTP_ASSOCINFO
    case 521: return SCTP_ASSOCINFO;
#endif
#ifdef SCTP_AUTHENTICATION
    case 522: return SCTP_AUTHENTICATION;
#endif
#ifdef SCTP_AUTH_ACTIVE_KEY
    case 523: return SCTP_AUTH_ACTIVE_KEY;
#endif
#ifdef SCTP_AUTH_CHUNK
    case 524: return SCTP_AUTH_CHUNK;
#endif
#ifdef SCTP_AUTH_DEACTIVATE_KEY
    case 525: return SCTP_AUTH_DEACTIVATE_KEY;
#endif
#ifdef SCTP_AUTH_DELETE_KEY
    case 526: return SCTP_AUTH_DELETE_KEY;
#endif
#ifdef SCTP_AUTH_KEY
    case 527: return SCTP_AUTH_KEY;
#endif
#ifdef SCTP_AUTH_SUPPORTED
    case 528: return SCTP_AUTH_SUPPORTED;
#endif
#ifdef SCTP_AUTOCLOSE
    case 529: return SCTP_AUTOCLOSE;
#endif
#ifdef SCTP_AUTO_ASCONF
    case 530: return SCTP_AUTO_ASCONF;
#endif
#ifdef SCTP_BADCRC
    case 531: return SCTP_BADCRC;
#endif
#ifdef SCTP_BINDX_ADD_ADDR
    case 532: return SCTP_BINDX_ADD_ADDR;
#endif
#ifdef SCTP_BINDX_REM_ADDR
    case 533: return SCTP_BINDX_REM_ADDR;
#endif
#ifdef SCTP_BLK_LOGGING_ENABLE
    case 534: return SCTP_BLK_LOGGING_ENABLE;
#endif
#ifdef SCTP_BOUND
    case 535: return SCTP_BOUND;
#endif
#ifdef SCTP_CAUSE_COOKIE_IN_SHUTDOWN
    case 536: return SCTP_CAUSE_COOKIE_IN_SHUTDOWN;
#endif
#ifdef SCTP_CAUSE_DELETING_LAST_ADDR
    case 537: return SCTP_CAUSE_DELETING_LAST_ADDR;
#endif
#ifdef SCTP_CAUSE_DELETING_SRC_ADDR
    case 538: return SCTP_CAUSE_DELETING_SRC_ADDR;
#endif
#ifdef SCTP_CAUSE_ILLEGAL_ASCONF_ACK
    case 539: return SCTP_CAUSE_ILLEGAL_ASCONF_ACK;
#endif
#ifdef SCTP_CAUSE_INVALID_PARAM
    case 540: return SCTP_CAUSE_INVALID_PARAM;
#endif
#ifdef SCTP_CAUSE_INVALID_STREAM
    case 541: return SCTP_CAUSE_INVALID_STREAM;
#endif
#ifdef SCTP_CAUSE_MISSING_PARAM
    case 542: return SCTP_CAUSE_MISSING_PARAM;
#endif
#ifdef SCTP_CAUSE_NAT_COLLIDING_STATE
    case 543: return SCTP_CAUSE_NAT_COLLIDING_STATE;
#endif
#ifdef SCTP_CAUSE_NAT_MISSING_STATE
    case 544: return SCTP_CAUSE_NAT_MISSING_STATE;
#endif
#ifdef SCTP_CAUSE_NO_ERROR
    case 545: return SCTP_CAUSE_NO_ERROR;
#endif
#ifdef SCTP_CAUSE_NO_USER_DATA
    case 546: return SCTP_CAUSE_NO_USER_DATA;
#endif
#ifdef SCTP_CAUSE_OUT_OF_RESC
    case 547: return SCTP_CAUSE_OUT_OF_RESC;
#endif
#ifdef SCTP_CAUSE_PROTOCOL_VIOLATION
    case 548: return SCTP_CAUSE_PROTOCOL_VIOLATION;
#endif
#ifdef SCTP_CAUSE_REQUEST_REFUSED
    case 549: return SCTP_CAUSE_REQUEST_REFUSED;
#endif
#ifdef SCTP_CAUSE_RESOURCE_SHORTAGE
    case 550: return SCTP_CAUSE_RESOURCE_SHORTAGE;
#endif
#ifdef SCTP_CAUSE_RESTART_W_NEWADDR
    case 551: return SCTP_CAUSE_RESTART_W_NEWADDR;
#endif
#ifdef SCTP_CAUSE_STALE_COOKIE
    case 552: return SCTP_CAUSE_STALE_COOKIE;
#endif
#ifdef SCTP_CAUSE_UNRECOG_CHUNK
    case 553: return SCTP_CAUSE_UNRECOG_CHUNK;
#endif
#ifdef SCTP_CAUSE_UNRECOG_PARAM
    case 554: return SCTP_CAUSE_UNRECOG_PARAM;
#endif
#ifdef SCTP_CAUSE_UNRESOLVABLE_ADDR
    case 555: return SCTP_CAUSE_UNRESOLVABLE_ADDR;
#endif
#ifdef SCTP_CAUSE_UNSUPPORTED_HMACID
    case 556: return SCTP_CAUSE_UNSUPPORTED_HMACID;
#endif
#ifdef SCTP_CAUSE_USER_INITIATED_ABT
    case 557: return SCTP_CAUSE_USER_INITIATED_ABT;
#endif
#ifdef SCTP_CC_HSTCP
    case 558: return SCTP_CC_HSTCP;
#endif
#ifdef SCTP_CC_HTCP
    case 559: return SCTP_CC_HTCP;
#endif
#ifdef SCTP_CC_OPTION
    case 560: return SCTP_CC_OPTION;
#endif
#ifdef SCTP_CC_OPT_RTCC_SETMODE
    case 561: return SCTP_CC_OPT_RTCC_SETMODE;
#endif
#ifdef SCTP_CC_OPT_STEADY_STEP
    case 562: return SCTP_CC_OPT_STEADY_STEP;
#endif
#ifdef SCTP_CC_OPT_USE_DCCC_ECN
    case 563: return SCTP_CC_OPT_USE_DCCC_ECN;
#endif
#ifdef SCTP_CC_RFC2581
    case 564: return SCTP_CC_RFC2581;
#endif
#ifdef SCTP_CC_RTCC
    case 565: return SCTP_CC_RTCC;
#endif
#ifdef SCTP_CLOSED
    case 566: return SCTP_CLOSED;
#endif
#ifdef SCTP_CLR_STAT_LOG
    case 567: return SCTP_CLR_STAT_LOG;
#endif
#ifdef SCTP_CMT_BASE
    case 568: return SCTP_CMT_BASE;
#endif
#ifdef SCTP_CMT_MAX
    case 569: return SCTP_CMT_MAX;
#endif
#ifdef SCTP_CMT_MPTCP
    case 570: return SCTP_CMT_MPTCP;
#endif
#ifdef SCTP_CMT_OFF
    case 571: return SCTP_CMT_OFF;
#endif
#ifdef SCTP_CMT_ON_OFF
    case 572: return SCTP_CMT_ON_OFF;
#endif
#ifdef SCTP_CMT_RPV1
    case 573: return SCTP_CMT_RPV1;
#endif
#ifdef SCTP_CMT_RPV2
    case 574: return SCTP_CMT_RPV2;
#endif
#ifdef SCTP_CMT_USE_DAC
    case 575: return SCTP_CMT_USE_DAC;
#endif
#ifdef SCTP_CONNECT_X
    case 576: return SCTP_CONNECT_X;
#endif
#ifdef SCTP_CONNECT_X_COMPLETE
    case 577: return SCTP_CONNECT_X_COMPLETE;
#endif
#ifdef SCTP_CONNECT_X_DELAYED
    case 578: return SCTP_CONNECT_X_DELAYED;
#endif
#ifdef SCTP_CONTEXT
    case 579: return SCTP_CONTEXT;
#endif
#ifdef SCTP_COOKIE_ACK
    case 580: return SCTP_COOKIE_ACK;
#endif
#ifdef SCTP_COOKIE_ECHO
    case 581: return SCTP_COOKIE_ECHO;
#endif
#ifdef SCTP_COOKIE_ECHOED
    case 582: return SCTP_COOKIE_ECHOED;
#endif
#ifdef SCTP_COOKIE_WAIT
    case 583: return SCTP_COOKIE_WAIT;
#endif
#ifdef SCTP_CWND_LOGGING_ENABLE
    case 584: return SCTP_CWND_LOGGING_ENABLE;
#endif
#ifdef SCTP_CWND_MONITOR_ENABLE
    case 585: return SCTP_CWND_MONITOR_ENABLE;
#endif
#ifdef SCTP_CWR_IN_SAME_WINDOW
    case 586: return SCTP_CWR_IN_SAME_WINDOW;
#endif
#ifdef SCTP_CWR_REDUCE_OVERRIDE
    case 587: return SCTP_CWR_REDUCE_OVERRIDE;
#endif
#ifdef SCTP_DATA
    case 588: return SCTP_DATA;
#endif
#ifdef SCTP_DATA_FIRST_FRAG
    case 589: return SCTP_DATA_FIRST_FRAG;
#endif
#ifdef SCTP_DATA_FRAG_MASK
    case 590: return SCTP_DATA_FRAG_MASK;
#endif
#ifdef SCTP_DATA_LAST_FRAG
    case 591: return SCTP_DATA_LAST_FRAG;
#endif
#ifdef SCTP_DATA_MIDDLE_FRAG
    case 592: return SCTP_DATA_MIDDLE_FRAG;
#endif
#ifdef SCTP_DATA_NOT_FRAG
    case 593: return SCTP_DATA_NOT_FRAG;
#endif
#ifdef SCTP_DATA_SACK_IMMEDIATELY
    case 594: return SCTP_DATA_SACK_IMMEDIATELY;
#endif
#ifdef SCTP_DATA_UNORDERED
    case 595: return SCTP_DATA_UNORDERED;
#endif
#ifdef SCTP_DEFAULT_PRINFO
    case 596: return SCTP_DEFAULT_PRINFO;
#endif
#ifdef SCTP_DEFAULT_SEND_PARAM
    case 597: return SCTP_DEFAULT_SEND_PARAM;
#endif
#ifdef SCTP_DEFAULT_SNDINFO
    case 598: return SCTP_DEFAULT_SNDINFO;
#endif
#ifdef SCTP_DELAYED_SACK
    case 599: return SCTP_DELAYED_SACK;
#endif
#ifdef SCTP_DEL_VRF_ID
    case 600: return SCTP_DEL_VRF_ID;
#endif
#ifdef SCTP_DISABLE_FRAGMENTS
    case 601: return SCTP_DISABLE_FRAGMENTS;
#endif
#ifdef SCTP_ECN_CWR
    case 602: return SCTP_ECN_CWR;
#endif
#ifdef SCTP_ECN_ECHO
    case 603: return SCTP_ECN_ECHO;
#endif
#ifdef SCTP_ECN_SUPPORTED
    case 604: return SCTP_ECN_SUPPORTED;
#endif
#ifdef SCTP_ENABLE_CHANGE_ASSOC_REQ
    case 605: return SCTP_ENABLE_CHANGE_ASSOC_REQ;
#endif
#ifdef SCTP_ENABLE_RESET_ASSOC_REQ
    case 606: return SCTP_ENABLE_RESET_ASSOC_REQ;
#endif
#ifdef SCTP_ENABLE_RESET_STREAM_REQ
    case 607: return SCTP_ENABLE_RESET_STREAM_REQ;
#endif
#ifdef SCTP_ENABLE_STREAM_RESET
    case 608: return SCTP_ENABLE_STREAM_RESET;
#endif
#ifdef SCTP_ENABLE_VALUE_MASK
    case 609: return SCTP_ENABLE_VALUE_MASK;
#endif
#ifdef SCTP_ESTABLISHED
    case 610: return SCTP_ESTABLISHED;
#endif
#ifdef SCTP_EVENT
    case 611: return SCTP_EVENT;
#endif
#ifdef SCTP_EVENTS
    case 612: return SCTP_EVENTS;
#endif
#ifdef SCTP_EXPLICIT_EOR
    case 613: return SCTP_EXPLICIT_EOR;
#endif
#ifdef SCTP_FLIGHT_LOGGING_ENABLE
    case 614: return SCTP_FLIGHT_LOGGING_ENABLE;
#endif
#ifdef SCTP_FORWARD_CUM_TSN
    case 615: return SCTP_FORWARD_CUM_TSN;
#endif
#ifdef SCTP_FRAGMENT_INTERLEAVE
    case 616: return SCTP_FRAGMENT_INTERLEAVE;
#endif
#ifdef SCTP_FRAG_LEVEL_0
    case 617: return SCTP_FRAG_LEVEL_0;
#endif
#ifdef SCTP_FRAG_LEVEL_1
    case 618: return SCTP_FRAG_LEVEL_1;
#endif
#ifdef SCTP_FRAG_LEVEL_2
    case 619: return SCTP_FRAG_LEVEL_2;
#endif
#ifdef SCTP_FROM_MIDDLE_BOX
    case 620: return SCTP_FROM_MIDDLE_BOX;
#endif
#ifdef SCTP_FR_LOGGING_ENABLE
    case 621: return SCTP_FR_LOGGING_ENABLE;
#endif
#ifdef SCTP_GET_ADDR_LEN
    case 622: return SCTP_GET_ADDR_LEN;
#endif
#ifdef SCTP_GET_ASOC_VRF
    case 623: return SCTP_GET_ASOC_VRF;
#endif
#ifdef SCTP_GET_ASSOC_ID_LIST
    case 624: return SCTP_GET_ASSOC_ID_LIST;
#endif
#ifdef SCTP_GET_ASSOC_NUMBER
    case 625: return SCTP_GET_ASSOC_NUMBER;
#endif
#ifdef SCTP_GET_LOCAL_ADDRESSES
    case 626: return SCTP_GET_LOCAL_ADDRESSES;
#endif
#ifdef SCTP_GET_LOCAL_ADDR_SIZE
    case 627: return SCTP_GET_LOCAL_ADDR_SIZE;
#endif
#ifdef SCTP_GET_NONCE_VALUES
    case 628: return SCTP_GET_NONCE_VALUES;
#endif
#ifdef SCTP_GET_PACKET_LOG
    case 629: return SCTP_GET_PACKET_LOG;
#endif
#ifdef SCTP_GET_PEER_ADDRESSES
    case 630: return SCTP_GET_PEER_ADDRESSES;
#endif
#ifdef SCTP_GET_PEER_ADDR_INFO
    case 631: return SCTP_GET_PEER_ADDR_INFO;
#endif
#ifdef SCTP_GET_REMOTE_ADDR_SIZE
    case 632: return SCTP_GET_REMOTE_ADDR_SIZE;
#endif
#ifdef SCTP_GET_SNDBUF_USE
    case 633: return SCTP_GET_SNDBUF_USE;
#endif
#ifdef SCTP_GET_STAT_LOG
    case 634: return SCTP_GET_STAT_LOG;
#endif
#ifdef SCTP_GET_VRF_IDS
    case 635: return SCTP_GET_VRF_IDS;
#endif
#ifdef SCTP_HAD_NO_TCB
    case 636: return SCTP_HAD_NO_TCB;
#endif
#ifdef SCTP_HEARTBEAT_ACK
    case 637: return SCTP_HEARTBEAT_ACK;
#endif
#ifdef SCTP_HEARTBEAT_REQUEST
    case 638: return SCTP_HEARTBEAT_REQUEST;
#endif
#ifdef SCTP_HMAC_IDENT
    case 639: return SCTP_HMAC_IDENT;
#endif
#ifdef SCTP_IDATA
    case 640: return SCTP_IDATA;
#endif
#ifdef SCTP_IFORWARD_CUM_TSN
    case 641: return SCTP_IFORWARD_CUM_TSN;
#endif
#ifdef SCTP_INITIATION
    case 642: return SCTP_INITIATION;
#endif
#ifdef SCTP_INITIATION_ACK
    case 643: return SCTP_INITIATION_ACK;
#endif
#ifdef SCTP_INITMSG
    case 644: return SCTP_INITMSG;
#endif
#ifdef SCTP_INTERLEAVING_SUPPORTED
    case 645: return SCTP_INTERLEAVING_SUPPORTED;
#endif
#ifdef SCTP_I_WANT_MAPPED_V4_ADDR
    case 646: return SCTP_I_WANT_MAPPED_V4_ADDR;
#endif
#ifdef SCTP_LAST_PACKET_TRACING
    case 647: return SCTP_LAST_PACKET_TRACING;
#endif
#ifdef SCTP_LISTEN
    case 648: return SCTP_LISTEN;
#endif
#ifdef SCTP_LOCAL_AUTH_CHUNKS
    case 649: return SCTP_LOCAL_AUTH_CHUNKS;
#endif
#ifdef SCTP_LOCK_LOGGING_ENABLE
    case 650: return SCTP_LOCK_LOGGING_ENABLE;
#endif
#ifdef SCTP_LOG_AT_SEND_2_OUTQ
    case 651: return SCTP_LOG_AT_SEND_2_OUTQ;
#endif
#ifdef SCTP_LOG_AT_SEND_2_SCTP
    case 652: return SCTP_LOG_AT_SEND_2_SCTP;
#endif
#ifdef SCTP_LOG_MAXBURST_ENABLE
    case 653: return SCTP_LOG_MAXBURST_ENABLE;
#endif
#ifdef SCTP_LOG_RWND_ENABLE
    case 654: return SCTP_LOG_RWND_ENABLE;
#endif
#ifdef SCTP_LOG_SACK_ARRIVALS_ENABLE
    case 655: return SCTP_LOG_SACK_ARRIVALS_ENABLE;
#endif
#ifdef SCTP_LOG_TRY_ADVANCE
    case 656: return SCTP_LOG_TRY_ADVANCE;
#endif
#ifdef SCTP_LTRACE_CHUNK_ENABLE
    case 657: return SCTP_LTRACE_CHUNK_ENABLE;
#endif
#ifdef SCTP_LTRACE_ERROR_ENABLE
    case 658: return SCTP_LTRACE_ERROR_ENABLE;
#endif
#ifdef SCTP_MAP_LOGGING_ENABLE
    case 659: return SCTP_MAP_LOGGING_ENABLE;
#endif
#ifdef SCTP_MAXBURST
    case 660: return SCTP_MAXBURST;
#endif
#ifdef SCTP_MAXSEG
    case 661: return SCTP_MAXSEG;
#endif
#ifdef SCTP_MAX_BURST
    case 662: return SCTP_MAX_BURST;
#endif
#ifdef SCTP_MAX_COOKIE_LIFE
    case 663: return SCTP_MAX_COOKIE_LIFE;
#endif
#ifdef SCTP_MAX_CWND
    case 664: return SCTP_MAX_CWND;
#endif
#ifdef SCTP_MAX_HB_INTERVAL
    case 665: return SCTP_MAX_HB_INTERVAL;
#endif
#ifdef SCTP_MAX_SACK_DELAY
    case 666: return SCTP_MAX_SACK_DELAY;
#endif
#ifdef SCTP_MBCNT_LOGGING_ENABLE
    case 667: return SCTP_MBCNT_LOGGING_ENABLE;
#endif
#ifdef SCTP_MBUF_LOGGING_ENABLE
    case 668: return SCTP_MBUF_LOGGING_ENABLE;
#endif
#ifdef SCTP_MOBILITY_BASE
    case 669: return SCTP_MOBILITY_BASE;
#endif
#ifdef SCTP_MOBILITY_FASTHANDOFF
    case 670: return SCTP_MOBILITY_FASTHANDOFF;
#endif
#ifdef SCTP_MOBILITY_PRIM_DELETED
    case 671: return SCTP_MOBILITY_PRIM_DELETED;
#endif
#ifdef SCTP_NAGLE_LOGGING_ENABLE
    case 672: return SCTP_NAGLE_LOGGING_ENABLE;
#endif
#ifdef SCTP_NODELAY
    case 673: return SCTP_NODELAY;
#endif
#ifdef SCTP_NRSACK_SUPPORTED
    case 674: return SCTP_NRSACK_SUPPORTED;
#endif
#ifdef SCTP_NR_SELECTIVE_ACK
    case 675: return SCTP_NR_SELECTIVE_ACK;
#endif
#ifdef SCTP_OPERATION_ERROR
    case 676: return SCTP_OPERATION_ERROR;
#endif
#ifdef SCTP_PACKED
    case 677: return SCTP_PACKED;
#endif
#ifdef SCTP_PACKET_DROPPED
    case 678: return SCTP_PACKET_DROPPED;
#endif
#ifdef SCTP_PACKET_LOG_SIZE
    case 679: return SCTP_PACKET_LOG_SIZE;
#endif
#ifdef SCTP_PACKET_TRUNCATED
    case 680: return SCTP_PACKET_TRUNCATED;
#endif
#ifdef SCTP_PAD_CHUNK
    case 681: return SCTP_PAD_CHUNK;
#endif
#ifdef SCTP_PARTIAL_DELIVERY_POINT
    case 682: return SCTP_PARTIAL_DELIVERY_POINT;
#endif
#ifdef SCTP_PCB_COPY_FLAGS
    case 683: return SCTP_PCB_COPY_FLAGS;
#endif
#ifdef SCTP_PCB_FLAGS_ACCEPTING
    case 684: return SCTP_PCB_FLAGS_ACCEPTING;
#endif
#ifdef SCTP_PCB_FLAGS_ADAPTATIONEVNT
    case 685: return SCTP_PCB_FLAGS_ADAPTATIONEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_ASSOC_RESETEVNT
    case 686: return SCTP_PCB_FLAGS_ASSOC_RESETEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_AUTHEVNT
    case 687: return SCTP_PCB_FLAGS_AUTHEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_AUTOCLOSE
    case 688: return SCTP_PCB_FLAGS_AUTOCLOSE;
#endif
#ifdef SCTP_PCB_FLAGS_AUTO_ASCONF
    case 689: return SCTP_PCB_FLAGS_AUTO_ASCONF;
#endif
#ifdef SCTP_PCB_FLAGS_BLOCKING_IO
    case 690: return SCTP_PCB_FLAGS_BLOCKING_IO;
#endif
#ifdef SCTP_PCB_FLAGS_BOUNDALL
    case 691: return SCTP_PCB_FLAGS_BOUNDALL;
#endif
#ifdef SCTP_PCB_FLAGS_BOUND_V6
    case 692: return SCTP_PCB_FLAGS_BOUND_V6;
#endif
#ifdef SCTP_PCB_FLAGS_CLOSE_IP
    case 693: return SCTP_PCB_FLAGS_CLOSE_IP;
#endif
#ifdef SCTP_PCB_FLAGS_CONNECTED
    case 694: return SCTP_PCB_FLAGS_CONNECTED;
#endif
#ifdef SCTP_PCB_FLAGS_DONOT_HEARTBEAT
    case 695: return SCTP_PCB_FLAGS_DONOT_HEARTBEAT;
#endif
#ifdef SCTP_PCB_FLAGS_DONT_WAKE
    case 696: return SCTP_PCB_FLAGS_DONT_WAKE;
#endif
#ifdef SCTP_PCB_FLAGS_DO_ASCONF
    case 697: return SCTP_PCB_FLAGS_DO_ASCONF;
#endif
#ifdef SCTP_PCB_FLAGS_DO_NOT_PMTUD
    case 698: return SCTP_PCB_FLAGS_DO_NOT_PMTUD;
#endif
#ifdef SCTP_PCB_FLAGS_DRYEVNT
    case 699: return SCTP_PCB_FLAGS_DRYEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_EXPLICIT_EOR
    case 700: return SCTP_PCB_FLAGS_EXPLICIT_EOR;
#endif
#ifdef SCTP_PCB_FLAGS_EXT_RCVINFO
    case 701: return SCTP_PCB_FLAGS_EXT_RCVINFO;
#endif
#ifdef SCTP_PCB_FLAGS_FRAG_INTERLEAVE
    case 702: return SCTP_PCB_FLAGS_FRAG_INTERLEAVE;
#endif
#ifdef SCTP_PCB_FLAGS_INTERLEAVE_STRMS
    case 703: return SCTP_PCB_FLAGS_INTERLEAVE_STRMS;
#endif
#ifdef SCTP_PCB_FLAGS_IN_TCPPOOL
    case 704: return SCTP_PCB_FLAGS_IN_TCPPOOL;
#endif
#ifdef SCTP_PCB_FLAGS_MULTIPLE_ASCONFS
    case 705: return SCTP_PCB_FLAGS_MULTIPLE_ASCONFS;
#endif
#ifdef SCTP_PCB_FLAGS_NEEDS_MAPPED_V4
    case 706: return SCTP_PCB_FLAGS_NEEDS_MAPPED_V4;
#endif
#ifdef SCTP_PCB_FLAGS_NODELAY
    case 707: return SCTP_PCB_FLAGS_NODELAY;
#endif
#ifdef SCTP_PCB_FLAGS_NO_FRAGMENT
    case 708: return SCTP_PCB_FLAGS_NO_FRAGMENT;
#endif
#ifdef SCTP_PCB_FLAGS_PDAPIEVNT
    case 709: return SCTP_PCB_FLAGS_PDAPIEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_PORTREUSE
    case 710: return SCTP_PCB_FLAGS_PORTREUSE;
#endif
#ifdef SCTP_PCB_FLAGS_RECVASSOCEVNT
    case 711: return SCTP_PCB_FLAGS_RECVASSOCEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_RECVDATAIOEVNT
    case 712: return SCTP_PCB_FLAGS_RECVDATAIOEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_RECVNSENDFAILEVNT
    case 713: return SCTP_PCB_FLAGS_RECVNSENDFAILEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_RECVNXTINFO
    case 714: return SCTP_PCB_FLAGS_RECVNXTINFO;
#endif
#ifdef SCTP_PCB_FLAGS_RECVPADDREVNT
    case 715: return SCTP_PCB_FLAGS_RECVPADDREVNT;
#endif
#ifdef SCTP_PCB_FLAGS_RECVPEERERR
    case 716: return SCTP_PCB_FLAGS_RECVPEERERR;
#endif
#ifdef SCTP_PCB_FLAGS_RECVRCVINFO
    case 717: return SCTP_PCB_FLAGS_RECVRCVINFO;
#endif
#ifdef SCTP_PCB_FLAGS_RECVSENDFAILEVNT
    case 718: return SCTP_PCB_FLAGS_RECVSENDFAILEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT
    case 719: return SCTP_PCB_FLAGS_RECVSHUTDOWNEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_SOCKET_ALLGONE
    case 720: return SCTP_PCB_FLAGS_SOCKET_ALLGONE;
#endif
#ifdef SCTP_PCB_FLAGS_SOCKET_CANT_READ
    case 721: return SCTP_PCB_FLAGS_SOCKET_CANT_READ;
#endif
#ifdef SCTP_PCB_FLAGS_SOCKET_GONE
    case 722: return SCTP_PCB_FLAGS_SOCKET_GONE;
#endif
#ifdef SCTP_PCB_FLAGS_STREAM_CHANGEEVNT
    case 723: return SCTP_PCB_FLAGS_STREAM_CHANGEEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_STREAM_RESETEVNT
    case 724: return SCTP_PCB_FLAGS_STREAM_RESETEVNT;
#endif
#ifdef SCTP_PCB_FLAGS_TCPTYPE
    case 725: return SCTP_PCB_FLAGS_TCPTYPE;
#endif
#ifdef SCTP_PCB_FLAGS_UDPTYPE
    case 726: return SCTP_PCB_FLAGS_UDPTYPE;
#endif
#ifdef SCTP_PCB_FLAGS_UNBOUND
    case 727: return SCTP_PCB_FLAGS_UNBOUND;
#endif
#ifdef SCTP_PCB_FLAGS_WAKEINPUT
    case 728: return SCTP_PCB_FLAGS_WAKEINPUT;
#endif
#ifdef SCTP_PCB_FLAGS_WAKEOUTPUT
    case 729: return SCTP_PCB_FLAGS_WAKEOUTPUT;
#endif
#ifdef SCTP_PCB_FLAGS_WAS_ABORTED
    case 730: return SCTP_PCB_FLAGS_WAS_ABORTED;
#endif
#ifdef SCTP_PCB_FLAGS_WAS_CONNECTED
    case 731: return SCTP_PCB_FLAGS_WAS_CONNECTED;
#endif
#ifdef SCTP_PCB_FLAGS_ZERO_COPY_ACTIVE
    case 732: return SCTP_PCB_FLAGS_ZERO_COPY_ACTIVE;
#endif
#ifdef SCTP_PCB_STATUS
    case 733: return SCTP_PCB_STATUS;
#endif
#ifdef SCTP_PEELOFF
    case 734: return SCTP_PEELOFF;
#endif
#ifdef SCTP_PEER_ADDR_PARAMS
    case 735: return SCTP_PEER_ADDR_PARAMS;
#endif
#ifdef SCTP_PEER_ADDR_THLDS
    case 736: return SCTP_PEER_ADDR_THLDS;
#endif
#ifdef SCTP_PEER_AUTH_CHUNKS
    case 737: return SCTP_PEER_AUTH_CHUNKS;
#endif
#ifdef SCTP_PKTDROP_SUPPORTED
    case 738: return SCTP_PKTDROP_SUPPORTED;
#endif
#ifdef SCTP_PLUGGABLE_CC
    case 739: return SCTP_PLUGGABLE_CC;
#endif
#ifdef SCTP_PLUGGABLE_SS
    case 740: return SCTP_PLUGGABLE_SS;
#endif
#ifdef SCTP_PRIMARY_ADDR
    case 741: return SCTP_PRIMARY_ADDR;
#endif
#ifdef SCTP_PR_ASSOC_STATUS
    case 742: return SCTP_PR_ASSOC_STATUS;
#endif
#ifdef SCTP_PR_STREAM_STATUS
    case 743: return SCTP_PR_STREAM_STATUS;
#endif
#ifdef SCTP_PR_SUPPORTED
    case 744: return SCTP_PR_SUPPORTED;
#endif
#ifdef SCTP_RECONFIG_SUPPORTED
    case 745: return SCTP_RECONFIG_SUPPORTED;
#endif
#ifdef SCTP_RECVNXTINFO
    case 746: return SCTP_RECVNXTINFO;
#endif
#ifdef SCTP_RECVRCVINFO
    case 747: return SCTP_RECVRCVINFO;
#endif
#ifdef SCTP_RECV_RWND_LOGGING_ENABLE
    case 748: return SCTP_RECV_RWND_LOGGING_ENABLE;
#endif
#ifdef SCTP_REMOTE_UDP_ENCAPS_PORT
    case 749: return SCTP_REMOTE_UDP_ENCAPS_PORT;
#endif
#ifdef SCTP_RESET_ASSOC
    case 750: return SCTP_RESET_ASSOC;
#endif
#ifdef SCTP_RESET_STREAMS
    case 751: return SCTP_RESET_STREAMS;
#endif
#ifdef SCTP_REUSE_PORT
    case 752: return SCTP_REUSE_PORT;
#endif
#ifdef SCTP_RTOINFO
    case 753: return SCTP_RTOINFO;
#endif
#ifdef SCTP_RTTVAR_LOGGING_ENABLE
    case 754: return SCTP_RTTVAR_LOGGING_ENABLE;
#endif
#ifdef SCTP_SACK_CMT_DAC
    case 755: return SCTP_SACK_CMT_DAC;
#endif
#ifdef SCTP_SACK_LOGGING_ENABLE
    case 756: return SCTP_SACK_LOGGING_ENABLE;
#endif
#ifdef SCTP_SACK_NONCE_SUM
    case 757: return SCTP_SACK_NONCE_SUM;
#endif
#ifdef SCTP_SACK_RWND_LOGGING_ENABLE
    case 758: return SCTP_SACK_RWND_LOGGING_ENABLE;
#endif
#ifdef SCTP_SAT_NETWORK_BURST_INCR
    case 759: return SCTP_SAT_NETWORK_BURST_INCR;
#endif
#ifdef SCTP_SAT_NETWORK_MIN
    case 760: return SCTP_SAT_NETWORK_MIN;
#endif
#ifdef SCTP_SB_LOGGING_ENABLE
    case 761: return SCTP_SB_LOGGING_ENABLE;
#endif
#ifdef SCTP_SELECTIVE_ACK
    case 762: return SCTP_SELECTIVE_ACK;
#endif
#ifdef SCTP_SET_DEBUG_LEVEL
    case 763: return SCTP_SET_DEBUG_LEVEL;
#endif
#ifdef SCTP_SET_DYNAMIC_PRIMARY
    case 764: return SCTP_SET_DYNAMIC_PRIMARY;
#endif
#ifdef SCTP_SET_INITIAL_DBG_SEQ
    case 765: return SCTP_SET_INITIAL_DBG_SEQ;
#endif
#ifdef SCTP_SET_PEER_PRIMARY_ADDR
    case 766: return SCTP_SET_PEER_PRIMARY_ADDR;
#endif
#ifdef SCTP_SHUTDOWN
    case 767: return SCTP_SHUTDOWN;
#endif
#ifdef SCTP_SHUTDOWN_ACK
    case 768: return SCTP_SHUTDOWN_ACK;
#endif
#ifdef SCTP_SHUTDOWN_ACK_SENT
    case 769: return SCTP_SHUTDOWN_ACK_SENT;
#endif
#ifdef SCTP_SHUTDOWN_COMPLETE
    case 770: return SCTP_SHUTDOWN_COMPLETE;
#endif
#ifdef SCTP_SHUTDOWN_PENDING
    case 771: return SCTP_SHUTDOWN_PENDING;
#endif
#ifdef SCTP_SHUTDOWN_RECEIVED
    case 772: return SCTP_SHUTDOWN_RECEIVED;
#endif
#ifdef SCTP_SHUTDOWN_SENT
    case 773: return SCTP_SHUTDOWN_SENT;
#endif
#ifdef SCTP_SMALLEST_PMTU
    case 774: return SCTP_SMALLEST_PMTU;
#endif
#ifdef SCTP_SS_DEFAULT
    case 775: return SCTP_SS_DEFAULT;
#endif
#ifdef SCTP_SS_FAIR_BANDWITH
    case 776: return SCTP_SS_FAIR_BANDWITH;
#endif
#ifdef SCTP_SS_FIRST_COME
    case 777: return SCTP_SS_FIRST_COME;
#endif
#ifdef SCTP_SS_PRIORITY
    case 778: return SCTP_SS_PRIORITY;
#endif
#ifdef SCTP_SS_ROUND_ROBIN
    case 779: return SCTP_SS_ROUND_ROBIN;
#endif
#ifdef SCTP_SS_ROUND_ROBIN_PACKET
    case 780: return SCTP_SS_ROUND_ROBIN_PACKET;
#endif
#ifdef SCTP_SS_VALUE
    case 781: return SCTP_SS_VALUE;
#endif
#ifdef SCTP_STATUS
    case 782: return SCTP_STATUS;
#endif
#ifdef SCTP_STREAM_RESET
    case 783: return SCTP_STREAM_RESET;
#endif
#ifdef SCTP_STREAM_RESET_INCOMING
    case 784: return SCTP_STREAM_RESET_INCOMING;
#endif
#ifdef SCTP_STREAM_RESET_OUTGOING
    case 785: return SCTP_STREAM_RESET_OUTGOING;
#endif
#ifdef SCTP_STR_LOGGING_ENABLE
    case 786: return SCTP_STR_LOGGING_ENABLE;
#endif
#ifdef SCTP_THRESHOLD_LOGGING
    case 787: return SCTP_THRESHOLD_LOGGING;
#endif
#ifdef SCTP_TIMEOUTS
    case 788: return SCTP_TIMEOUTS;
#endif
#ifdef SCTP_USE_EXT_RCVINFO
    case 789: return SCTP_USE_EXT_RCVINFO;
#endif
#ifdef SCTP_VRF_ID
    case 790: return SCTP_VRF_ID;
#endif
#ifdef SCTP_WAKE_LOGGING_ENABLE
    case 791: return SCTP_WAKE_LOGGING_ENABLE;
#endif
#ifdef SOCK_CLOEXEC
    case 792: return SOCK_CLOEXEC;
#endif
#ifdef SOCK_DGRAM
    case 793: return SOCK_DGRAM;
#endif
#ifdef SOCK_MAXADDRLEN
    case 794: return SOCK_MAXADDRLEN;
#endif
#ifdef SOCK_NONBLOCK
    case 795: return SOCK_NONBLOCK;
#endif
#ifdef SOCK_RAW
    case 796: return SOCK_RAW;
#endif
#ifdef SOCK_RDM
    case 797: return SOCK_RDM;
#endif
#ifdef SOCK_SEQPACKET
    case 798: return SOCK_SEQPACKET;
#endif
#ifdef SOCK_STREAM
    case 799: return SOCK_STREAM;
#endif
#ifdef SOMAXCONN
    case 800: return SOMAXCONN;
#endif
#ifdef SONPX_SETOPTSHUT
    case 801: return SONPX_SETOPTSHUT;
#endif
#ifdef SO_ACCEPTCONN
    case 802: return SO_ACCEPTCONN;
#endif
#ifdef SO_ACCEPTFILTER
    case 803: return SO_ACCEPTFILTER;
#endif
#ifdef SO_ATMPVC
    case 804: return SO_ATMPVC;
#endif
#ifdef SO_ATMQOS
    case 805: return SO_ATMQOS;
#endif
#ifdef SO_ATMSAP
    case 806: return SO_ATMSAP;
#endif
#ifdef SO_ATTACH_BPF
    case 807: return SO_ATTACH_BPF;
#endif
#ifdef SO_ATTACH_FILTER
    case 808: return SO_ATTACH_FILTER;
#endif
#ifdef SO_BINDTODEVICE
    case 809: return SO_BINDTODEVICE;
#endif
#ifdef SO_BINTIME
    case 810: return SO_BINTIME;
#endif
#ifdef SO_BPF_EXTENSIONS
    case 811: return SO_BPF_EXTENSIONS;
#endif
#ifdef SO_BROADCAST
    case 812: return SO_BROADCAST;
#endif
#ifdef SO_BSDCOMPAT
    case 813: return SO_BSDCOMPAT;
#endif
#ifdef SO_BSP_STATE
    case 814: return SO_BSP_STATE;
#endif
#ifdef SO_BUSY_POLL
    case 815: return SO_BUSY_POLL;
#endif
#ifdef SO_CONACCESS
    case 816: return SO_CONACCESS;
#endif
#ifdef SO_CONDATA
    case 817: return SO_CONDATA;
#endif
#ifdef SO_CONDITIONAL_ACCEPT
    case 818: return SO_CONDITIONAL_ACCEPT;
#endif
#ifdef SO_CONNECT_TIME
    case 819: return SO_CONNECT_TIME;
#endif
#ifdef SO_DEBUG
    case 820: return SO_DEBUG;
#endif
#ifdef SO_DETACH_BPF
    case 821: return SO_DETACH_BPF;
#endif
#ifdef SO_DETACH_FILTER
    case 822: return SO_DETACH_FILTER;
#endif
#ifdef SO_DOMAIN
    case 823: return SO_DOMAIN;
#endif
#ifdef SO_DONTLINGER
    case 824: return SO_DONTLINGER;
#endif
#ifdef SO_DONTROUTE
    case 825: return SO_DONTROUTE;
#endif
#ifdef SO_DONTTRUNC
    case 826: return SO_DONTTRUNC;
#endif
#ifdef SO_ERROR
    case 827: return SO_ERROR;
#endif
#ifdef SO_EXCLUSIVEADDRUSE
    case 828: return SO_EXCLUSIVEADDRUSE;
#endif
#ifdef SO_GET_FILTER
    case 829: return SO_GET_FILTER;
#endif
#ifdef SO_GROUP_ID
    case 830: return SO_GROUP_ID;
#endif
#ifdef SO_GROUP_PRIORITY
    case 831: return SO_GROUP_PRIORITY;
#endif
#ifdef SO_HCI_RAW_DIRECTION
    case 832: return SO_HCI_RAW_DIRECTION;
#endif
#ifdef SO_HCI_RAW_FILTER
    case 833: return SO_HCI_RAW_FILTER;
#endif
#ifdef SO_INCOMING_CPU
    case 834: return SO_INCOMING_CPU;
#endif
#ifdef SO_KEEPALIVE
    case 835: return SO_KEEPALIVE;
#endif
#ifdef SO_L2CAP_ENCRYPTED
    case 836: return SO_L2CAP_ENCRYPTED;
#endif
#ifdef SO_L2CAP_FLUSH
    case 837: return SO_L2CAP_FLUSH;
#endif
#ifdef SO_L2CAP_IFLOW
    case 838: return SO_L2CAP_IFLOW;
#endif
#ifdef SO_L2CAP_IMTU
    case 839: return SO_L2CAP_IMTU;
#endif
#ifdef SO_L2CAP_OFLOW
    case 840: return SO_L2CAP_OFLOW;
#endif
#ifdef SO_L2CAP_OMTU
    case 841: return SO_L2CAP_OMTU;
#endif
#ifdef SO_LABEL
    case 842: return SO_LABEL;
#endif
#ifdef SO_LINGER
    case 843: return SO_LINGER;
#endif
#ifdef SO_LINGER_SEC
    case 844: return SO_LINGER_SEC;
#endif
#ifdef SO_LINKINFO
    case 845: return SO_LINKINFO;
#endif
#ifdef SO_LISTENINCQLEN
    case 846: return SO_LISTENINCQLEN;
#endif
#ifdef SO_LISTENQLEN
    case 847: return SO_LISTENQLEN;
#endif
#ifdef SO_LISTENQLIMIT
    case 848: return SO_LISTENQLIMIT;
#endif
#ifdef SO_LOCK_FILTER
    case 849: return SO_LOCK_FILTER;
#endif
#ifdef SO_MARK
    case 850: return SO_MARK;
#endif
#ifdef SO_MAX_MSG_SIZE
    case 851: return SO_MAX_MSG_SIZE;
#endif
#ifdef SO_MAX_PACING_RATE
    case 852: return SO_MAX_PACING_RATE;
#endif
#ifdef SO_MULTIPOINT
    case 853: return SO_MULTIPOINT;
#endif
#ifdef SO_NETSVC_MARKING_LEVEL
    case 854: return SO_NETSVC_MARKING_LEVEL;
#endif
#ifdef SO_NET_SERVICE_TYPE
    case 855: return SO_NET_SERVICE_TYPE;
#endif
#ifdef SO_NKE
    case 856: return SO_NKE;
#endif
#ifdef SO_NOADDRERR
    case 857: return SO_NOADDRERR;
#endif
#ifdef SO_NOFCS
    case 858: return SO_NOFCS;
#endif
#ifdef SO_NOSIGPIPE
    case 859: return SO_NOSIGPIPE;
#endif
#ifdef SO_NOTIFYCONFLICT
    case 860: return SO_NOTIFYCONFLICT;
#endif
#ifdef SO_NO_CHECK
    case 861: return SO_NO_CHECK;
#endif
#ifdef SO_NO_DDP
    case 862: return SO_NO_DDP;
#endif
#ifdef SO_NO_OFFLOAD
    case 863: return SO_NO_OFFLOAD;
#endif
#ifdef SO_NP_EXTENSIONS
    case 864: return SO_NP_EXTENSIONS;
#endif
#ifdef SO_NREAD
    case 865: return SO_NREAD;
#endif
#ifdef SO_NUMRCVPKT
    case 866: return SO_NUMRCVPKT;
#endif
#ifdef SO_NWRITE
    case 867: return SO_NWRITE;
#endif
#ifdef SO_OOBINLINE
    case 868: return SO_OOBINLINE;
#endif
#ifdef SO_ORIGINAL_DST
    case 869: return SO_ORIGINAL_DST;
#endif
#ifdef SO_PASSCRED
    case 870: return SO_PASSCRED;
#endif
#ifdef SO_PASSSEC
    case 871: return SO_PASSSEC;
#endif
#ifdef SO_PEEK_OFF
    case 872: return SO_PEEK_OFF;
#endif
#ifdef SO_PEERCRED
    case 873: return SO_PEERCRED;
#endif
#ifdef SO_PEERLABEL
    case 874: return SO_PEERLABEL;
#endif
#ifdef SO_PEERNAME
    case 875: return SO_PEERNAME;
#endif
#ifdef SO_PEERSEC
    case 876: return SO_PEERSEC;
#endif
#ifdef SO_PORT_SCALABILITY
    case 877: return SO_PORT_SCALABILITY;
#endif
#ifdef SO_PRIORITY
    case 878: return SO_PRIORITY;
#endif
#ifdef SO_PROTOCOL
    case 879: return SO_PROTOCOL;
#endif
#ifdef SO_PROTOCOL_INFO
    case 880: return SO_PROTOCOL_INFO;
#endif
#ifdef SO_PROTOTYPE
    case 881: return SO_PROTOTYPE;
#endif
#ifdef SO_PROXYUSR
    case 882: return SO_PROXYUSR;
#endif
#ifdef SO_RANDOMPORT
    case 883: return SO_RANDOMPORT;
#endif
#ifdef SO_RCVBUF
    case 884: return SO_RCVBUF;
#endif
#ifdef SO_RCVBUFFORCE
    case 885: return SO_RCVBUFFORCE;
#endif
#ifdef SO_RCVLOWAT
    case 886: return SO_RCVLOWAT;
#endif
#ifdef SO_RCVTIMEO
#  ifndef SO_RCVTIMEO_OLD
#    define SO_RCVTIMEO_OLD 20 // for musl
#  endif
    case 887: return SO_RCVTIMEO;
#endif
#ifdef SO_RDS_TRANSPORT
    case 888: return SO_RDS_TRANSPORT;
#endif
#ifdef SO_REUSEADDR
    case 889: return SO_REUSEADDR;
#endif
#ifdef SO_REUSEPORT
    case 890: return SO_REUSEPORT;
#endif
#ifdef SO_REUSESHAREUID
    case 891: return SO_REUSESHAREUID;
#endif
#ifdef SO_RFCOMM_FC_INFO
    case 892: return SO_RFCOMM_FC_INFO;
#endif
#ifdef SO_RFCOMM_MTU
    case 893: return SO_RFCOMM_MTU;
#endif
#ifdef SO_RXQ_OVFL
    case 894: return SO_RXQ_OVFL;
#endif
#ifdef SO_SCO_CONNINFO
    case 895: return SO_SCO_CONNINFO;
#endif
#ifdef SO_SCO_MTU
    case 896: return SO_SCO_MTU;
#endif
#ifdef SO_SECURITY_AUTHENTICATION
    case 897: return SO_SECURITY_AUTHENTICATION;
#endif
#ifdef SO_SECURITY_ENCRYPTION_NETWORK
    case 898: return SO_SECURITY_ENCRYPTION_NETWORK;
#endif
#ifdef SO_SECURITY_ENCRYPTION_TRANSPORT
    case 899: return SO_SECURITY_ENCRYPTION_TRANSPORT;
#endif
#ifdef SO_SELECT_ERR_QUEUE
    case 900: return SO_SELECT_ERR_QUEUE;
#endif
#ifdef SO_SETCLP
    case 901: return SO_SETCLP;
#endif
#ifdef SO_SETFIB
    case 902: return SO_SETFIB;
#endif
#ifdef SO_SNDBUF
    case 903: return SO_SNDBUF;
#endif
#ifdef SO_SNDBUFFORCE
    case 904: return SO_SNDBUFFORCE;
#endif
#ifdef SO_SNDLOWAT
    case 905: return SO_SNDLOWAT;
#endif
#ifdef SO_SNDTIMEO
#  ifndef SO_SNDTIMEO_OLD
#    define SO_SNDTIMEO_OLD 21 // for musl
#  endif
    case 906: return SO_SNDTIMEO;
#endif
#ifdef SO_TIMESTAMP
    case 907: return SO_TIMESTAMP;
#endif
#ifdef SO_TIMESTAMPING
    case 908: return SO_TIMESTAMPING;
#endif
#ifdef SO_TIMESTAMPNS
    case 909: return SO_TIMESTAMPNS;
#endif
#ifdef SO_TIMESTAMP_MONOTONIC
    case 910: return SO_TIMESTAMP_MONOTONIC;
#endif
#ifdef SO_TYPE
    case 911: return SO_TYPE;
#endif
#ifdef SO_UPCALLCLOSEWAIT
    case 912: return SO_UPCALLCLOSEWAIT;
#endif
#ifdef SO_UPDATE_ACCEPT_CONTEXT
    case 913: return SO_UPDATE_ACCEPT_CONTEXT;
#endif
#ifdef SO_USELOOPBACK
    case 914: return SO_USELOOPBACK;
#endif
#ifdef SO_USER_COOKIE
    case 915: return SO_USER_COOKIE;
#endif
#ifdef SO_VENDOR
    case 916: return SO_VENDOR;
#endif
#ifdef SO_VM_SOCKETS_BUFFER_MAX_SIZE
    case 917: return SO_VM_SOCKETS_BUFFER_MAX_SIZE;
#endif
#ifdef SO_VM_SOCKETS_BUFFER_MIN_SIZE
    case 918: return SO_VM_SOCKETS_BUFFER_MIN_SIZE;
#endif
#ifdef SO_VM_SOCKETS_BUFFER_SIZE
    case 919: return SO_VM_SOCKETS_BUFFER_SIZE;
#endif
#ifdef SO_VM_SOCKETS_CONNECT_TIMEOUT
    case 920: return SO_VM_SOCKETS_CONNECT_TIMEOUT;
#endif
#ifdef SO_VM_SOCKETS_NONBLOCK_TXRX
    case 921: return SO_VM_SOCKETS_NONBLOCK_TXRX;
#endif
#ifdef SO_VM_SOCKETS_PEER_HOST_VM_ID
    case 922: return SO_VM_SOCKETS_PEER_HOST_VM_ID;
#endif
#ifdef SO_VM_SOCKETS_TRUSTED
    case 923: return SO_VM_SOCKETS_TRUSTED;
#endif
#ifdef SO_WANTMORE
    case 924: return SO_WANTMORE;
#endif
#ifdef SO_WANTOOBFLAG
    case 925: return SO_WANTOOBFLAG;
#endif
#ifdef SO_WIFI_STATUS
    case 926: return SO_WIFI_STATUS;
#endif
#ifdef TCP6_MSS
    case 927: return TCP6_MSS;
#endif
#ifdef TCPCI_FLAG_LOSSRECOVERY
    case 928: return TCPCI_FLAG_LOSSRECOVERY;
#endif
#ifdef TCPCI_FLAG_REORDERING_DETECTED
    case 929: return TCPCI_FLAG_REORDERING_DETECTED;
#endif
#ifdef TCPCI_OPT_ECN
    case 930: return TCPCI_OPT_ECN;
#endif
#ifdef TCPCI_OPT_SACK
    case 931: return TCPCI_OPT_SACK;
#endif
#ifdef TCPCI_OPT_TIMESTAMPS
    case 932: return TCPCI_OPT_TIMESTAMPS;
#endif
#ifdef TCPCI_OPT_WSCALE
    case 933: return TCPCI_OPT_WSCALE;
#endif
#ifdef TCPF_CA_CWR
    case 934: return TCPF_CA_CWR;
#endif
#ifdef TCPF_CA_Disorder
    case 935: return TCPF_CA_Disorder;
#endif
#ifdef TCPF_CA_Loss
    case 936: return TCPF_CA_Loss;
#endif
#ifdef TCPF_CA_Open
    case 937: return TCPF_CA_Open;
#endif
#ifdef TCPF_CA_Recovery
    case 938: return TCPF_CA_Recovery;
#endif
#ifdef TCPI_OPT_ECN
    case 939: return TCPI_OPT_ECN;
#endif
#ifdef TCPI_OPT_ECN_SEEN
    case 940: return TCPI_OPT_ECN_SEEN;
#endif
#ifdef TCPI_OPT_SACK
    case 941: return TCPI_OPT_SACK;
#endif
#ifdef TCPI_OPT_SYN_DATA
    case 942: return TCPI_OPT_SYN_DATA;
#endif
#ifdef TCPI_OPT_TIMESTAMPS
    case 943: return TCPI_OPT_TIMESTAMPS;
#endif
#ifdef TCPI_OPT_TOE
    case 944: return TCPI_OPT_TOE;
#endif
#ifdef TCPI_OPT_WSCALE
    case 945: return TCPI_OPT_WSCALE;
#endif
#ifdef TCPOLEN_CC
    case 946: return TCPOLEN_CC;
#endif
#ifdef TCPOLEN_CC_APPA
    case 947: return TCPOLEN_CC_APPA;
#endif
#ifdef TCPOLEN_EOL
    case 948: return TCPOLEN_EOL;
#endif
#ifdef TCPOLEN_FASTOPEN_REQ
    case 949: return TCPOLEN_FASTOPEN_REQ;
#endif
#ifdef TCPOLEN_FAST_OPEN_EMPTY
    case 950: return TCPOLEN_FAST_OPEN_EMPTY;
#endif
#ifdef TCPOLEN_FAST_OPEN_MAX
    case 951: return TCPOLEN_FAST_OPEN_MAX;
#endif
#ifdef TCPOLEN_FAST_OPEN_MIN
    case 952: return TCPOLEN_FAST_OPEN_MIN;
#endif
#ifdef TCPOLEN_MAXSEG
    case 953: return TCPOLEN_MAXSEG;
#endif
#ifdef TCPOLEN_NOP
    case 954: return TCPOLEN_NOP;
#endif
#ifdef TCPOLEN_PAD
    case 955: return TCPOLEN_PAD;
#endif
#ifdef TCPOLEN_SACK
    case 956: return TCPOLEN_SACK;
#endif
#ifdef TCPOLEN_SACKHDR
    case 957: return TCPOLEN_SACKHDR;
#endif
#ifdef TCPOLEN_SACK_PERMITTED
    case 958: return TCPOLEN_SACK_PERMITTED;
#endif
#ifdef TCPOLEN_SIGNATURE
    case 959: return TCPOLEN_SIGNATURE;
#endif
#ifdef TCPOLEN_TIMESTAMP
    case 960: return TCPOLEN_TIMESTAMP;
#endif
#ifdef TCPOLEN_TSTAMP_APPA
    case 961: return TCPOLEN_TSTAMP_APPA;
#endif
#ifdef TCPOLEN_WINDOW
    case 962: return TCPOLEN_WINDOW;
#endif
#ifdef TCPOPT_CC
    case 963: return TCPOPT_CC;
#endif
#ifdef TCPOPT_CCECHO
    case 964: return TCPOPT_CCECHO;
#endif
#ifdef TCPOPT_CCNEW
    case 965: return TCPOPT_CCNEW;
#endif
#ifdef TCPOPT_EOL
    case 966: return TCPOPT_EOL;
#endif
#ifdef TCPOPT_FASTOPEN
    case 967: return TCPOPT_FASTOPEN;
#endif
#ifdef TCPOPT_FAST_OPEN
    case 968: return TCPOPT_FAST_OPEN;
#endif
#ifdef TCPOPT_MAXSEG
    case 969: return TCPOPT_MAXSEG;
#endif
#ifdef TCPOPT_MULTIPATH
    case 970: return TCPOPT_MULTIPATH;
#endif
#ifdef TCPOPT_NOP
    case 971: return TCPOPT_NOP;
#endif
#ifdef TCPOPT_PAD
    case 972: return TCPOPT_PAD;
#endif
#ifdef TCPOPT_SACK
    case 973: return TCPOPT_SACK;
#endif
#ifdef TCPOPT_SACK_HDR
    case 974: return TCPOPT_SACK_HDR;
#endif
#ifdef TCPOPT_SACK_PERMITTED
    case 975: return TCPOPT_SACK_PERMITTED;
#endif
#ifdef TCPOPT_SACK_PERMIT_HDR
    case 976: return TCPOPT_SACK_PERMIT_HDR;
#endif
#ifdef TCPOPT_SIGNATURE
    case 977: return TCPOPT_SIGNATURE;
#endif
#ifdef TCPOPT_TIMESTAMP
    case 978: return TCPOPT_TIMESTAMP;
#endif
#ifdef TCPOPT_TSTAMP_HDR
    case 979: return TCPOPT_TSTAMP_HDR;
#endif
#ifdef TCPOPT_WINDOW
    case 980: return TCPOPT_WINDOW;
#endif
#ifdef TCP_CA_NAME_MAX
    case 981: return TCP_CA_NAME_MAX;
#endif
#ifdef TCP_CCALGOOPT
    case 982: return TCP_CCALGOOPT;
#endif
#ifdef TCP_CC_INFO
    case 983: return TCP_CC_INFO;
#endif
#ifdef TCP_CONGESTION
    case 984: return TCP_CONGESTION;
#endif
#ifdef TCP_CONNECTIONTIMEOUT
    case 985: return TCP_CONNECTIONTIMEOUT;
#endif
#ifdef TCP_CONNECTION_INFO
    case 986: return TCP_CONNECTION_INFO;
#endif
#ifdef TCP_COOKIE_IN_ALWAYS
    case 987: return TCP_COOKIE_IN_ALWAYS;
#endif
#ifdef TCP_COOKIE_MAX
    case 988: return TCP_COOKIE_MAX;
#endif
#ifdef TCP_COOKIE_MIN
    case 989: return TCP_COOKIE_MIN;
#endif
#ifdef TCP_COOKIE_OUT_NEVER
    case 990: return TCP_COOKIE_OUT_NEVER;
#endif
#ifdef TCP_COOKIE_PAIR_SIZE
    case 991: return TCP_COOKIE_PAIR_SIZE;
#endif
#ifdef TCP_COOKIE_TRANSACTIONS
    case 992: return TCP_COOKIE_TRANSACTIONS;
#endif
#ifdef TCP_CORK
    case 993: return TCP_CORK;
#endif
#ifdef TCP_DEFER_ACCEPT
    case 994: return TCP_DEFER_ACCEPT;
#endif
#ifdef TCP_ENABLE_ECN
    case 995: return TCP_ENABLE_ECN;
#endif
#ifdef TCP_FASTOPEN
    case 996: return TCP_FASTOPEN;
#endif
#ifdef TCP_FUNCTION_BLK
    case 997: return TCP_FUNCTION_BLK;
#endif
#ifdef TCP_FUNCTION_NAME_LEN_MAX
    case 998: return TCP_FUNCTION_NAME_LEN_MAX;
#endif
#ifdef TCP_INFO
    case 999: return TCP_INFO;
#endif
#ifdef TCP_KEEPALIVE
    case 1000: return TCP_KEEPALIVE;
#endif
#ifdef TCP_KEEPCNT
    case 1001: return TCP_KEEPCNT;
#endif
#ifdef TCP_KEEPIDLE
    case 1002: return TCP_KEEPIDLE;
#endif
#ifdef TCP_KEEPINIT
    case 1003: return TCP_KEEPINIT;
#endif
#ifdef TCP_KEEPINTVL
    case 1004: return TCP_KEEPINTVL;
#endif
#ifdef TCP_LINGER2
    case 1005: return TCP_LINGER2;
#endif
#ifdef TCP_MAXBURST
    case 1006: return TCP_MAXBURST;
#endif
#ifdef TCP_MAXHLEN
    case 1007: return TCP_MAXHLEN;
#endif
#ifdef TCP_MAXOLEN
    case 1008: return TCP_MAXOLEN;
#endif
#ifdef TCP_MAXSEG
    case 1009: return TCP_MAXSEG;
#endif
#ifdef TCP_MAXWIN
    case 1010: return TCP_MAXWIN;
#endif
#ifdef TCP_MAX_SACK
    case 1011: return TCP_MAX_SACK;
#endif
#ifdef TCP_MAX_WINSHIFT
    case 1012: return TCP_MAX_WINSHIFT;
#endif
#ifdef TCP_MD5SIG
    case 1013: return TCP_MD5SIG;
#endif
#ifdef TCP_MD5SIG_MAXKEYLEN
    case 1014: return TCP_MD5SIG_MAXKEYLEN;
#endif
#ifdef TCP_MINMSS
    case 1015: return TCP_MINMSS;
#endif
#ifdef TCP_MSS
    case 1016: return TCP_MSS;
#endif
#ifdef TCP_MSS_DEFAULT
    case 1017: return TCP_MSS_DEFAULT;
#endif
#ifdef TCP_MSS_DESIRED
    case 1018: return TCP_MSS_DESIRED;
#endif
#ifdef TCP_NODELAY
    case 1019: return TCP_NODELAY;
#endif
#ifdef TCP_NOOPT
    case 1020: return TCP_NOOPT;
#endif
#ifdef TCP_NOPUSH
    case 1021: return TCP_NOPUSH;
#endif
#ifdef TCP_NOTSENT_LOWAT
    case 1022: return TCP_NOTSENT_LOWAT;
#endif
#ifdef TCP_PCAP_IN
    case 1023: return TCP_PCAP_IN;
#endif
#ifdef TCP_PCAP_OUT
    case 1024: return TCP_PCAP_OUT;
#endif
#ifdef TCP_QUEUE_SEQ
    case 1025: return TCP_QUEUE_SEQ;
#endif
#ifdef TCP_QUICKACK
    case 1026: return TCP_QUICKACK;
#endif
#ifdef TCP_REPAIR
    case 1027: return TCP_REPAIR;
#endif
#ifdef TCP_REPAIR_OPTIONS
    case 1028: return TCP_REPAIR_OPTIONS;
#endif
#ifdef TCP_REPAIR_QUEUE
    case 1029: return TCP_REPAIR_QUEUE;
#endif
#ifdef TCP_RXT_CONNDROPTIME
    case 1030: return TCP_RXT_CONNDROPTIME;
#endif
#ifdef TCP_RXT_FINDROP
    case 1031: return TCP_RXT_FINDROP;
#endif
#ifdef TCP_SAVED_SYN
    case 1032: return TCP_SAVED_SYN;
#endif
#ifdef TCP_SAVE_SYN
    case 1033: return TCP_SAVE_SYN;
#endif
#ifdef TCP_SENDMOREACKS
    case 1034: return TCP_SENDMOREACKS;
#endif
#ifdef TCP_SYNCNT
    case 1035: return TCP_SYNCNT;
#endif
#ifdef TCP_S_DATA_IN
    case 1036: return TCP_S_DATA_IN;
#endif
#ifdef TCP_S_DATA_OUT
    case 1037: return TCP_S_DATA_OUT;
#endif
#ifdef TCP_THIN_DUPACK
    case 1038: return TCP_THIN_DUPACK;
#endif
#ifdef TCP_THIN_LINEAR_TIMEOUTS
    case 1039: return TCP_THIN_LINEAR_TIMEOUTS;
#endif
#ifdef TCP_TIMESTAMP
    case 1040: return TCP_TIMESTAMP;
#endif
#ifdef TCP_USER_TIMEOUT
    case 1041: return TCP_USER_TIMEOUT;
#endif
#ifdef TCP_VENDOR
    case 1042: return TCP_VENDOR;
#endif
#ifdef TCP_WINDOW_CLAMP
    case 1043: return TCP_WINDOW_CLAMP;
#endif
#ifdef TIPC_ADDR_ID
    case 1044: return TIPC_ADDR_ID;
#endif
#ifdef TIPC_ADDR_MCAST
    case 1045: return TIPC_ADDR_MCAST;
#endif
#ifdef TIPC_ADDR_NAME
    case 1046: return TIPC_ADDR_NAME;
#endif
#ifdef TIPC_ADDR_NAMESEQ
    case 1047: return TIPC_ADDR_NAMESEQ;
#endif
#ifdef TIPC_CFG_SRV
    case 1048: return TIPC_CFG_SRV;
#endif
#ifdef TIPC_CLUSTER_SCOPE
    case 1049: return TIPC_CLUSTER_SCOPE;
#endif
#ifdef TIPC_CONN_SHUTDOWN
    case 1050: return TIPC_CONN_SHUTDOWN;
#endif
#ifdef TIPC_CONN_TIMEOUT
    case 1051: return TIPC_CONN_TIMEOUT;
#endif
#ifdef TIPC_CRITICAL_IMPORTANCE
    case 1052: return TIPC_CRITICAL_IMPORTANCE;
#endif
#ifdef TIPC_DESTNAME
    case 1053: return TIPC_DESTNAME;
#endif
#ifdef TIPC_DEST_DROPPABLE
    case 1054: return TIPC_DEST_DROPPABLE;
#endif
#ifdef TIPC_ERRINFO
    case 1055: return TIPC_ERRINFO;
#endif
#ifdef TIPC_ERR_NO_NAME
    case 1056: return TIPC_ERR_NO_NAME;
#endif
#ifdef TIPC_ERR_NO_NODE
    case 1057: return TIPC_ERR_NO_NODE;
#endif
#ifdef TIPC_ERR_NO_PORT
    case 1058: return TIPC_ERR_NO_PORT;
#endif
#ifdef TIPC_ERR_OVERLOAD
    case 1059: return TIPC_ERR_OVERLOAD;
#endif
#ifdef TIPC_HIGH_IMPORTANCE
    case 1060: return TIPC_HIGH_IMPORTANCE;
#endif
#ifdef TIPC_IMPORTANCE
    case 1061: return TIPC_IMPORTANCE;
#endif
#ifdef TIPC_LINK_STATE
    case 1062: return TIPC_LINK_STATE;
#endif
#ifdef TIPC_LOW_IMPORTANCE
    case 1063: return TIPC_LOW_IMPORTANCE;
#endif
#ifdef TIPC_MAX_BEARER_NAME
    case 1064: return TIPC_MAX_BEARER_NAME;
#endif
#ifdef TIPC_MAX_IF_NAME
    case 1065: return TIPC_MAX_IF_NAME;
#endif
#ifdef TIPC_MAX_LINK_NAME
    case 1066: return TIPC_MAX_LINK_NAME;
#endif
#ifdef TIPC_MAX_MEDIA_NAME
    case 1067: return TIPC_MAX_MEDIA_NAME;
#endif
#ifdef TIPC_MAX_USER_MSG_SIZE
    case 1068: return TIPC_MAX_USER_MSG_SIZE;
#endif
#ifdef TIPC_MEDIUM_IMPORTANCE
    case 1069: return TIPC_MEDIUM_IMPORTANCE;
#endif
#ifdef TIPC_NODE_RECVQ_DEPTH
    case 1070: return TIPC_NODE_RECVQ_DEPTH;
#endif
#ifdef TIPC_NODE_SCOPE
    case 1071: return TIPC_NODE_SCOPE;
#endif
#ifdef TIPC_OK
    case 1072: return TIPC_OK;
#endif
#ifdef TIPC_PUBLISHED
    case 1073: return TIPC_PUBLISHED;
#endif
#ifdef TIPC_RESERVED_TYPES
    case 1074: return TIPC_RESERVED_TYPES;
#endif
#ifdef TIPC_RETDATA
    case 1075: return TIPC_RETDATA;
#endif
#ifdef TIPC_SOCK_RECVQ_DEPTH
    case 1076: return TIPC_SOCK_RECVQ_DEPTH;
#endif
#ifdef TIPC_SRC_DROPPABLE
    case 1077: return TIPC_SRC_DROPPABLE;
#endif
#ifdef TIPC_SUBSCR_TIMEOUT
    case 1078: return TIPC_SUBSCR_TIMEOUT;
#endif
#ifdef TIPC_SUB_CANCEL
    case 1079: return TIPC_SUB_CANCEL;
#endif
#ifdef TIPC_SUB_PORTS
    case 1080: return TIPC_SUB_PORTS;
#endif
#ifdef TIPC_SUB_SERVICE
    case 1081: return TIPC_SUB_SERVICE;
#endif
#ifdef TIPC_TOP_SRV
    case 1082: return TIPC_TOP_SRV;
#endif
#ifdef TIPC_WAIT_FOREVER
    case 1083: return TIPC_WAIT_FOREVER;
#endif
#ifdef TIPC_WITHDRAWN
    case 1084: return TIPC_WITHDRAWN;
#endif
#ifdef TIPC_ZONE_SCOPE
    case 1085: return TIPC_ZONE_SCOPE;
#endif
#ifdef TTCP_CLIENT_SND_WND
    case 1086: return TTCP_CLIENT_SND_WND;
#endif
#ifdef UDP_CORK
    case 1087: return UDP_CORK;
#endif
#ifdef UDP_ENCAP
    case 1088: return UDP_ENCAP;
#endif
#ifdef UDP_ENCAP_ESPINUDP
    case 1089: return UDP_ENCAP_ESPINUDP;
#endif
#ifdef UDP_ENCAP_ESPINUDP_MAXFRAGLEN
    case 1090: return UDP_ENCAP_ESPINUDP_MAXFRAGLEN;
#endif
#ifdef UDP_ENCAP_ESPINUDP_NON_IKE
    case 1091: return UDP_ENCAP_ESPINUDP_NON_IKE;
#endif
#ifdef UDP_ENCAP_ESPINUDP_PORT
    case 1092: return UDP_ENCAP_ESPINUDP_PORT;
#endif
#ifdef UDP_ENCAP_L2TPINUDP
    case 1093: return UDP_ENCAP_L2TPINUDP;
#endif
#ifdef UDP_NOCKSUM
    case 1094: return UDP_NOCKSUM;
#endif
#ifdef UDP_NO_CHECK6_RX
    case 1095: return UDP_NO_CHECK6_RX;
#endif
#ifdef UDP_NO_CHECK6_TX
    case 1096: return UDP_NO_CHECK6_TX;
#endif
#ifdef UDP_VENDOR
    case 1097: return UDP_VENDOR;
#endif
#ifdef SO_RCVTIMEO_OLD
    case 1098: return SO_RCVTIMEO_OLD;
#endif
#ifdef SO_RCVTIMEO_NEW
    case 1099: return SO_RCVTIMEO_NEW;
#endif
#ifdef SO_SNDTIMEO_OLD
    case 1100: return SO_SNDTIMEO_OLD;
#endif
#ifdef SO_SNDTIMEO_NEW
    case 1101: return SO_SNDTIMEO_NEW;
#endif
    default: return -1;
  }
}
PONY_EXTERN_C_END
