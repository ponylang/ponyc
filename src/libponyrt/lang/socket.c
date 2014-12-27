#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#include "lang.h"
#include "../asio/asio.h"
#include "../asio/event.h"
#include <stdbool.h>
#include <string.h>

#ifdef PLATFORM_IS_WINDOWS
#include <ws2tcpip.h>
#include <winsock2.h>
#else
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#endif

PONY_EXTERN_C_BEGIN

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

static int os_socket_addrinfo(struct addrinfo* p, bool server)
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

static int os_socket(const char* host, const char* service, int family,
  int socktype, int proto, bool server)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_flags = AI_ADDRCONFIG;
  hints.ai_family = family;
  hints.ai_socktype = socktype;
  hints.ai_protocol = proto;

  if(server)
    hints.ai_flags |= AI_PASSIVE;

  struct addrinfo *result;

  if(getaddrinfo(host, service, &hints, &result) != 0)
    return -1;

  struct addrinfo* p = result;
  int fd = -1;

  while(p != NULL)
  {
    fd = os_socket_addrinfo(p, server);

    if(fd != -1)
      break;

    p = p->ai_next;
  }

  freeaddrinfo(result);
  return fd;
}

int os_listen_tcp(const char* host, const char* service)
{
  return os_socket(host, service, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, true);
}

int os_listen_tcp4(const char* host, const char* service)
{
  return os_socket(host, service, AF_INET, SOCK_STREAM, IPPROTO_TCP, true);
}

int os_listen_tcp6(const char* host, const char* service)
{
  return os_socket(host, service, AF_INET6, SOCK_STREAM, IPPROTO_TCP, true);
}

int os_listen_udp(const char* host, const char* service)
{
  return os_socket(host, service, AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, true);
}

int os_listen_udp4(const char* host, const char* service)
{
  return os_socket(host, service, AF_INET, SOCK_DGRAM, IPPROTO_UDP, true);
}

int os_listen_udp6(const char* host, const char* service)
{
  return os_socket(host, service, AF_INET6, SOCK_DGRAM, IPPROTO_UDP, true);
}

int os_connect_tcp(const char* host, const char* service)
{
  return os_socket(host, service, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, false);
}

int os_connect_tcp4(const char* host, const char* service)
{
  return os_socket(host, service, AF_INET, SOCK_STREAM, IPPROTO_TCP, false);
}

int os_connect_tcp6(const char* host, const char* service)
{
  return os_socket(host, service, AF_INET6, SOCK_STREAM, IPPROTO_TCP, false);
}

asio_event_t* os_socket_event(pony_actor_t* handler, int fd)
{
  pony_type_t* type = *(pony_type_t**)handler;
  uint32_t msg_id = type->event_notify;

  if(msg_id == (uint32_t)-1)
    return NULL;

  asio_event_t* ev = asio_event_create(handler, msg_id, fd,
    ASIO_FILT_READ | ASIO_FILT_WRITE, true, NULL);

  asio_event_subscribe(ev);
  return ev;
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

static hostserv_t os_nameinfo(struct sockaddr_storage* addr, socklen_t len)
{
  char host[NI_MAXHOST];
  char serv[NI_MAXSERV];

  int r = getnameinfo((struct sockaddr*)addr, len, host, NI_MAXHOST,
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

hostserv_t os_sockname(int fd)
{
  SOCKET s = fd;
  struct sockaddr_storage addr;
  socklen_t len = sizeof(struct sockaddr_storage);

  if(getsockname(s, (struct sockaddr*)&addr, &len) != 0)
    pony_throw();

  return os_nameinfo(&addr, len);
}

hostserv_t os_peername(int fd)
{
  SOCKET s = fd;
  struct sockaddr_storage addr;
  socklen_t len = sizeof(struct sockaddr_storage);

  if(getpeername(s, (struct sockaddr*)&addr, &len) != 0)
    pony_throw();

  return os_nameinfo(&addr, len);
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

void os_closesocket(int fd)
{
#ifdef PLATFORM_IS_WINDOWS
  closesocket((SOCKET)fd);
#else
  close(fd);
#endif
}

PONY_EXTERN_C_END
