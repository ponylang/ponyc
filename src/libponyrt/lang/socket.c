#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <platform.h>

#include "../asio/asio.h"
#include "../asio/event.h"
#include <stdbool.h>
#include <string.h>

#ifdef PLATFORM_IS_WINDOWS
#include <ws2tcpip.h>
#include <winSock2.h>
#else
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#endif

PONY_EXTERN_C_BEGIN

int os_closesocket(int fd);

static bool connect_in_progress()
{
#ifdef PLATFORM_IS_WINDOWS
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EINPROGRESS;
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
  int n = getaddrinfo(host, service, &hints, &result);

  if(n < 0)
    return -1;

  struct addrinfo* p = result;
  SOCKET fd = -1;

  while(p != NULL)
  {
#ifdef PLATFORM_IS_LINUX
    fd = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, p->ai_protocol);
#else
    fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
#endif

    if(fd >= 0)
    {
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

          if((r == 0) && (socktype == SOCK_STREAM))
            r |= listen(fd, SOMAXCONN);
        }
        else {
          int ok = connect(fd, p->ai_addr, (int)p->ai_addrlen);

          if((ok != 0) && !connect_in_progress())
            r |= ok;
        }
      }

      if(r == 0)
        break;

      os_closesocket((int)fd);
      fd = -1;
    }

    p = p->ai_next;
  }

  freeaddrinfo(result);
  return (int)fd;
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
  SOCKET s = fd;

#ifdef PLATFORM_IS_LINUX
  SOCKET ns = accept4(s, NULL, NULL, SOCK_NONBLOCK);
#else
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

ssize_t os_send(int fd, const void* buf, size_t len)
{
  SOCKET s = fd;

#if defined(PLATFORM_IS_LINUX)
  return send(s, buf, len, MSG_NOSIGNAL);
#else
  return send(s, (const char*)buf, (int)len, 0);
#endif
}

int os_closesocket(int fd)
{
  SOCKET s = fd;

#ifdef PLATFORM_IS_WINDOWS
  return closesocket(s);
#else
  return close(s);
#endif
}

PONY_EXTERN_C_END
