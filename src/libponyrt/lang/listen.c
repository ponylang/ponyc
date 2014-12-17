#include "../asio/asio.h"
#include "../asio/event.h"
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

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
  int fd = -1;

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
        r |= setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));
      }

#ifdef PLATFORM_IS_MACOSX
      int nosigpipe = 1;
      r |= setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(int));
#endif

#ifndef PLATFORM_IS_LINUX
      int flags = fcntl(fd, F_GETFL, 0);
      r |= fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif

      if(r == 0)
      {
        if(server)
        {
          r |= bind(fd, p->ai_addr, p->ai_addrlen);

          if((r == 0) && (socktype == SOCK_STREAM))
            r |= listen(fd, SOMAXCONN);
        } else {
          int ok = connect(fd, p->ai_addr, p->ai_addrlen);

          if((ok != 0) && (errno != EINPROGRESS))
            r |= ok;
        }
      }

      if(r == 0)
        break;

      close(fd);
      fd = -1;
    }

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

asio_event_t* os_socket_event(int fd, uint32_t msg_id)
{
  asio_event_t* ev = asio_event_create(fd, ASIO_FILT_READ | ASIO_FILT_WRITE,
    msg_id, true, NULL);

  asio_event_subscribe(ev);
  return ev;
}

int os_accept(int fd)
{
#ifdef PLATFORM_IS_LINUX
  int s = accept4(fd, NULL, NULL, SOCK_NONBLOCK);
#else
  int s = accept(fd, NULL, NULL);

  if(s != -1)
  {
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
  }
#endif

  return s;
}

// Check this when a connection gets its first writeable event.
bool os_connected(int fd)
{
  int val = 0;
  socklen_t len = sizeof(int);

  if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len) == -1)
    return false;

  return val == 0;
}

ssize_t os_send(int fd, const void* buf, size_t len)
{
#if defined(PLATFORM_IS_LINUX)
  return send(fd, buf, len, MSG_NOSIGNAL);
#elif defined(PLATFORM_IS_MACOSX)
  return send(fd, buf, len, 0);
#endif
}
