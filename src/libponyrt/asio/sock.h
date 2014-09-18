#ifndef asio_sock_h
#define asio_sock_h

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/** Opaque definition of a socket.
 *
 */
typedef struct sock_t sock_t;

/** Get the OS-Level file descriptor of a socket.
 *
 */
intptr_t sock_get_fd(sock_t* s);

/** Creates a listener socket for the specified service.
 *
 *  The backlog determines the size of the buffer for incomming connections.
 *  Once the buffer is exhausted the underlying protocol may trigger reconnects.
 */
sock_t* sock_listen(const char* service, uint32_t backlog);

/** Accept a new connection.
 *
 *  Returns ASIO_SUCCESS, ASIO_WOULDBLOCK or ASIO_ERROR.
 */
uint32_t sock_accept(sock_t* listener, sock_t** accepted);

/** Establish a connection to some remote peer.
 *
 *  Returns ASIO_SUCCESS, ASIO_WOULDBLOCK or ASIO_ERROR. If a call to connect
 *  would block, the caller should subscribe for writability.
 */
uint32_t sock_connect(const char* host, const char* port, sock_t** connected);

/** Returns an index starting from which the caller can write at least "len"
 *  bytes to the socket.
 *
 *  The index can be used at any point in time before flushing, even if data has
 *  been written to the socket after bookmarking.
 */
size_t sock_bookmark(sock_t* sock, size_t len);

/** Write data to a previously bookmarked location of the socket.
 *
 */
void sock_bookmark_write(sock_t* sock, size_t index, void* data, size_t len);

/** Read data from the network.
 *
 *  Returns the status of the operation (ASIO_ERROR, ASIO_WOULDBLOCK, ASIO_SUCCESS).
 */
uint32_t sock_read(sock_t* sock);

/** Gets up to "len" bytes from the pending read list.
 *
 */
void sock_get_data(sock_t* sock, void* dest, size_t len);

/** Append data to the pending write list of a socket.
 *
 */
void sock_write(sock_t* sock, void* data, size_t len);

/** Flush a socket's buffer to the network.
 *
 *  Returns the status of the requested operation.
 */
uint32_t sock_flush(sock_t* s);

/** Returns the number of bytes available the be retrieved from the socket.
 *
 */
size_t sock_read_buffer_size(sock_t* s);

/** Close a socket, if no pending writes exist.
 *
 *  Closing a socket does not cause correlated I/O-events to be unsubscribes.
 *  events should be unsubscribed before closing a socket.
 *
 *  Returns true if the socket was closed.
 */
bool sock_close(sock_t* sock);

#endif
