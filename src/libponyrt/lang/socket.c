#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#include "lang.h"
#include "../asio/asio.h"
#include "../asio/event.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#ifdef PLATFORM_IS_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
typedef int SOCKET;
#endif

PONY_EXTERN_C_BEGIN

asio_event_t* os_socket_event(pony_actor_t* owner, int fd);
void os_closesocket(int fd);

static bool connect_in_progress()
{
#ifdef PLATFORM_IS_WINDOWS
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EINPROGRESS;
#endif
}

static bool would_block()
{
#ifdef PLATFORM_IS_WINDOWS
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EWOULDBLOCK;
#endif
}

#ifndef PLATFORM_IS_LINUX
static int set_nonblocking(SOCKET s)
{
#ifdef PLATFORM_IS_WINDOWS
  u_long flag = 1;
  return ioctlsocket(s, FIONBIO, &flag);
#else
  int flags = fcntl(s, F_GETFL, 0);
  return fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}
#endif

static int socket_from_addrinfo(struct addrinfo* p, bool server)
{
#ifdef PLATFORM_IS_LINUX
  int fd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol);
#else
  SOCKET fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#endif

  if(fd < 0)
    return -1;

  int r = 0;

  if(server)
  {
    int reuseaddr = 1;
    r |= setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
      (const char*)&reuseaddr, sizeof(int));
  }

#ifdef PLATFORM_IS_MACOSX
  int nosigpipe = 1;
  r |= setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(int));
#endif

#ifndef PLATFORM_IS_LINUX
  r |= set_nonblocking(fd);
#endif

  if(r == 0)
  {
    if(server)
    {
      r |= bind(fd, p->ai_addr, (int)p->ai_addrlen);

      if((r == 0) && (p->ai_socktype == SOCK_STREAM))
        r |= listen(fd, SOMAXCONN);
    } else {
      int ok = connect(fd, p->ai_addr, (int)p->ai_addrlen);

      if((ok != 0) && !connect_in_progress())
        r |= ok;
    }
  }

  if(r == 0)
    return fd;

  os_closesocket((int)fd);
  return -1;
}

/**
 * For a server, this finds an address to listen on and returns either a valid
 * file descriptor or -1. For a client, this starts Happy Eyeballs and returns
 * the number of connection attempts in-flight, which may be 0.
 */
static int os_socket(pony_actor_t* owner, const char* host,
  const char* service, int family, int socktype, int proto, bool server)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = family;
  hints.ai_socktype = socktype;
  hints.ai_protocol = proto;

  if(server)
    hints.ai_flags |= AI_PASSIVE;

  if((host != NULL) && (host[0] == '\0'))
    host = NULL;

  struct addrinfo *result;

  if(getaddrinfo(host, service, &hints, &result) != 0)
    return server ? -1 : 0;

  struct addrinfo* p = result;
  int count = 0;

  while(p != NULL)
  {
    int fd = socket_from_addrinfo(p, server);

    if(fd != -1)
    {
      asio_event_t* ev = os_socket_event(owner, fd);

      if(server)
      {
        // Send the event to servers, so that it can be unsubscribed before any
        // connections are accepted.
        asio_event_send(ev, ASIO_READ);
        freeaddrinfo(result);
        return fd;
      }

      count++;
    }

    p = p->ai_next;
  }

  freeaddrinfo(result);
  return count;
}

asio_event_t* os_socket_event(pony_actor_t* owner, int fd)
{
  pony_type_t* type = *(pony_type_t**)owner;
  uint32_t msg_id = type->event_notify;

  if(msg_id == (uint32_t)-1)
    return NULL;

  asio_event_t* ev = asio_event_create(owner, msg_id, fd,
    ASIO_READ | ASIO_WRITE, true, NULL);

  asio_event_subscribe(ev);
  return ev;
}

int os_socket_event_fd(asio_event_t* ev)
{
  return (int)ev->fd;
}

int os_listen_tcp(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP,
    true);
}

int os_listen_tcp4(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_INET, SOCK_STREAM, IPPROTO_TCP,
    true);
}

int os_listen_tcp6(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_INET6, SOCK_STREAM, IPPROTO_TCP,
    true);
}

int os_listen_udp(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP,
    true);
}

int os_listen_udp4(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_INET, SOCK_DGRAM, IPPROTO_UDP,
    true);
}

int os_listen_udp6(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_INET6, SOCK_DGRAM, IPPROTO_UDP,
    true);
}

int os_connect_tcp(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP,
    false);
}

int os_connect_tcp4(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_INET, SOCK_STREAM, IPPROTO_TCP,
    false);
}

int os_connect_tcp6(pony_actor_t* owner, const char* host, const char* service)
{
  return os_socket(owner, host, service, AF_INET6, SOCK_STREAM, IPPROTO_TCP,
    false);
}

int os_accept(int fd)
{
#ifdef PLATFORM_IS_LINUX
  int ns = accept4(fd, NULL, NULL, SOCK_NONBLOCK);
#else
  SOCKET s = fd;
  SOCKET ns = accept(s, NULL, NULL);

  if(ns != -1)
    set_nonblocking(ns);
#endif

  return (int)ns;
}

// Check this when a connection gets its first writeable event.
bool os_connected(int fd)
{
  SOCKET s = fd;
  int val = 0;
  socklen_t len = sizeof(int);

  if(getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&val, &len) == -1)
    return false;

  return val == 0;
}

typedef struct
{
  char* host;
  char* serv;
} hostserv_t;

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

    default:
      pony_throw();
  }

  return 0;
}

hostserv_t os_nameinfo(ipaddress_t* ipaddr)
{
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];

  socklen_t len = address_length(ipaddr);

  int r = getnameinfo((struct sockaddr*)&ipaddr->addr, len, host, NI_MAXHOST,
    serv, NI_MAXSERV, 0);

  if(r != 0)
    pony_throw();

  hostserv_t h;

  size_t hostlen = strlen(host);
  h.host = (char*)pony_alloc(hostlen + 1);
  memcpy(h.host, host, hostlen + 1);

  size_t servlen = strlen(serv);
  h.serv = (char*)pony_alloc(servlen + 1);
  memcpy(h.serv, serv, servlen + 1);

  return h;
}

struct addrinfo* os_addrinfo(int family, const char* host, const char* service)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;
  hints.ai_family = family;

  if((host != NULL) && (host[0] == '\0'))
    host = NULL;

  struct addrinfo *result;

  if(getaddrinfo(host, service, &hints, &result) != 0)
    return NULL;

  return result;
}

void os_getaddr(struct addrinfo* addr, ipaddress_t* ipaddr)
{
  memcpy(&ipaddr->addr, addr->ai_addr, addr->ai_addrlen);
}

struct addrinfo* os_nextaddr(struct addrinfo* addr)
{
  return addr->ai_next;
}

void os_sockname(int fd, ipaddress_t* ipaddr)
{
  SOCKET s = fd;
  socklen_t len = sizeof(struct sockaddr_storage);
  getsockname(s, (struct sockaddr*)&ipaddr->addr, &len);
}

void os_peername(int fd, ipaddress_t* ipaddr)
{
  SOCKET s = fd;
  socklen_t len = sizeof(struct sockaddr_storage);
  getpeername(s, (struct sockaddr*)&ipaddr->addr, &len);
}

size_t os_send(int fd, const void* buf, size_t len)
{
#if defined(PLATFORM_IS_LINUX)
  ssize_t sent = send(fd, buf, len, MSG_NOSIGNAL);
#else
  SOCKET s = fd;
  ssize_t sent = send(s, (const char*)buf, (int)len, 0);
#endif

  if(sent < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  }

  return (size_t)sent;
}

size_t os_recv(int fd, void* buf, size_t len)
{
  SOCKET s = fd;
  ssize_t recvd = recv(s, (char*)buf, (int)len, 0);

  if(recvd < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  } else if(recvd == 0) {
    pony_throw();
  }

  return (size_t)recvd;
}

size_t os_sendto(int fd, const void* buf, size_t len, ipaddress_t* ipaddr)
{
  socklen_t addrlen = address_length(ipaddr);

#if defined(PLATFORM_IS_LINUX)
  ssize_t sent = sendto(fd, buf, len, MSG_NOSIGNAL,
    (struct sockaddr*)&ipaddr->addr, addrlen);
#else
  SOCKET s = fd;
  ssize_t sent = sendto(s, (const char*)buf, (int)len, 0,
    (struct sockaddr*)&ipaddr->addr, addrlen);
#endif

  if(sent < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  }

  return (size_t)sent;
}

size_t os_recvfrom(int fd, void* buf, size_t len, ipaddress_t* ipaddr)
{
  SOCKET s = fd;
  socklen_t addrlen = sizeof(struct sockaddr_storage);

  ssize_t recvd = recvfrom(s, (char*)buf, (int)len, 0,
    (struct sockaddr*)&ipaddr->addr, &addrlen);

  if(recvd < 0)
  {
    if(would_block())
      return 0;

    pony_throw();
  } else if(recvd == 0) {
    pony_throw();
  }

  return (size_t)recvd;
}

void os_keepalive(int fd, int secs)
{
  SOCKET s = fd;
  int on = (secs > 0) ? 1 : 0;
  setsockopt(s, SOL_SOCKET,  SO_KEEPALIVE, (const char*)&on, sizeof(int));

  if(on == 0)
    return;

#if defined(PLATFORM_IS_LINUX)
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

  WSAIoctl(s, SIO_KEEPALIVE_VALS, sizeof(struct tcp_keepalive), NULL, 0, &ret,
    NULL, NULL);
#endif
}

void os_nodelay(int fd, bool state)
{
  SOCKET s = fd;
  int val = state;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&val, sizeof(int));
}

void os_closesocket(int fd)
{
#ifdef PLATFORM_IS_WINDOWS
  closesocket((SOCKET)fd);
#else
  close(fd);
#endif
}

PONY_EXTERN_C_END
