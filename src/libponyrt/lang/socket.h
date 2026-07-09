#ifndef lang_socket_h
#define lang_socket_h

#include <platform.h>
#include <stdint.h>

#ifdef PLATFORM_IS_WINDOWS
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#include "../asio/event.h"
#include "../pony.h"

PONY_EXTERN_C_BEGIN

// Wire-level result for the five PONY_API socket runtime functions
// (pony_os_writev, pony_os_send, pony_os_recv, pony_os_sendto,
// pony_os_recvfrom). The integer values are part of the ABI:
//
//   PONY_SOCKET_OK    = 0  operation completed; *count_out holds the byte
//                          count read/written for the caller to act on.
//                          These calls are synchronous and non-blocking on
//                          every platform (the Windows backend uses readiness
//                          notifications, not overlapped IOCP), so the count
//                          is always the bytes transferred by this call.
//                          A datagram pony_os_recvfrom may return OK with a 0
//                          count: an empty UDP datagram (RFC 768) is a valid
//                          read, unlike stream pony_os_recv where a 0-byte read
//                          means the peer closed (ERROR, below).
//   PONY_SOCKET_RETRY = 1  transient condition (POSIX EWOULDBLOCK/EAGAIN,
//                          Windows WSAEWOULDBLOCK). Caller should retry later.
//                          *count_out is 0.
//   PONY_SOCKET_ERROR = 2  unrecoverable error or peer closed (POSIX recv
//                          returning 0). *count_out is 0.
//
// The functions ALWAYS write *count_out before returning; callers MUST
// pass a non-NULL pointer to a writable size_t. Passing NULL will
// segfault.
//
// On RETRY and ERROR, pony_os_recvfrom does not write *ipaddr; treat its
// contents as unspecified.
//
// Pony stdlib decodes these values in packages/net/_socket_result.pony.
// Keep the integer values in sync with the _SocketResult* primitives'
// apply() returns there.
//
// The C type is uint8_t (not a C enum) to keep the FFI return width
// pinned at one byte — the Pony FFI declares the return type as [U8].
typedef uint8_t pony_socket_result_t;
#define PONY_SOCKET_OK    ((pony_socket_result_t)0)
#define PONY_SOCKET_RETRY ((pony_socket_result_t)1)
#define PONY_SOCKET_ERROR ((pony_socket_result_t)2)

// This must match the pony NetAddress type in packages/net.
typedef struct
{
  pony_type_t* type;
  struct sockaddr_storage addr;
} ipaddress_t;

PONY_API pony_socket_result_t pony_os_recv(asio_event_t* ev, char* buf,
  size_t len, size_t* count_out);

PONY_API pony_socket_result_t pony_os_recvfrom(asio_event_t* ev, char* buf,
  size_t len, ipaddress_t* ipaddr, size_t* count_out);

bool ponyint_os_sockets_init();

void ponyint_os_sockets_final();

PONY_EXTERN_C_END

#endif
