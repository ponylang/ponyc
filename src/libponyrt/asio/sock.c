#include "sock.h"
#include "asio.h"
#include "../mem/pool.h"

#include <string.h>

#ifndef PLATFORM_IS_WINDOWS

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netdb.h>
#include <errno.h>

#ifndef SOCK_NONBLOCK
#  define SOCK_NONBLOCK 0
#endif

#define CHUNK_SIZE 1024
#define BUFFER_SIZE (CHUNK_SIZE - sizeof(void*))

enum
{
	CONNECT,
	BIND
};

typedef ssize_t op_fn(int fd, const struct iovec *iov, int iovcnt);

typedef struct sock_chunk_t
{
	void* next;
	unsigned char data[BUFFER_SIZE];
} sock_chunk_t;

typedef struct chunk_list_t
{
	sock_chunk_t* head;
	sock_chunk_t* tail;

	void* readp;
	void* writep;

	int chunks;
	size_t size;
} chunk_list_t;

struct sock_t
{
	int fd;

	chunk_list_t reads;
	chunk_list_t writes;

	uint32_t status;
};

static size_t bytes_left(void* offset, sock_chunk_t* chunk)
{
	return BUFFER_SIZE - (offset - (void*)chunk->data);
}

static size_t bytes_available(void* offset, sock_chunk_t* chunk)
{
	return (offset - (void*)chunk->data);
}

static void append_chunk(chunk_list_t* list)
{
	sock_chunk_t* n = POOL_ALLOC(sock_chunk_t);
	memset(n, 0, sizeof(sock_chunk_t));

	list->chunks++;
	list->writep = n->data;

	if(list->head == NULL)
	{
		list->head = list->tail = n;
		return;
	}

	list->tail->next = n;
	list->tail = n;
}

static void set_non_blocking(int fd)
{
	uint32_t flags = 0;

	#if !SOCK_NONBLOCK
	#if defined(__linux__) || defined(__APPLE__)
	flags = fcntl(fd, F_GETFL, 0);
	flags |= O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);
	#endif //TODO WINDOWS
	#endif
}

/** Wrapper for writev and readv.
 */
static uint32_t exec(op_fn* fn, int fd, struct iovec* iov, int chunks,
	size_t* nrp)
{
	ssize_t ret;
	*nrp = 0;

	ret = fn(fd, iov, chunks);

	if(ret < 0)
		return (errno == EWOULDBLOCK) ? ASIO_WOULDBLOCK : ASIO_ERROR;

  if(nrp != NULL) *nrp += ret;

  return ASIO_SUCCESS;
}

static uint32_t sock_writev(int fd, struct iovec* iov, int chunks, size_t* nrp)
{
	return exec(writev, fd, iov, chunks, nrp);
}

static uint32_t sock_readv(int fd, struct iovec* iov, int chunks, size_t* nrp)
{
	return exec(readv, fd, iov, chunks, nrp);
}

static uint32_t sock_read1(int fd, void* dest, size_t len, size_t* nrp)
{
	struct iovec iov[1];

	iov[0].iov_len = len;
  iov[0].iov_base = dest;

	return sock_readv(fd, iov, 1, nrp);
}

static sock_t* create_socket(const char* host, const char* service,
	uint32_t mode)
{
	sock_t* s = POOL_ALLOC(sock_t);

	struct addrinfo hints, *res, *curr;

	int32_t rc = 0;
	uint32_t option = 1;

	memset(&hints, 0, sizeof(struct addrinfo));
	memset(s, 0, sizeof(sock_t));

	hints.ai_family = AF_UNSPEC;			// Allow IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;      // Wildcard IP
	hints.ai_protocol = IPPROTO_TCP;

  if(getaddrinfo(host, service, &hints, &res) != 0)
	{
		POOL_FREE(sock_t, s);
		return NULL;
	}

	for(curr = res; curr != NULL; curr = curr->ai_next)
	{
		s->fd = socket(curr->ai_family, curr->ai_socktype | SOCK_NONBLOCK,
			curr->ai_protocol);

		if(s->fd == -1)
			continue;

		setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

		if(mode == CONNECT)
		{
			rc = connect(s->fd, curr->ai_addr, curr->ai_addrlen);
			break;
		}
		else if(mode == BIND)
		{
			rc = bind(s->fd, curr->ai_addr, curr->ai_addrlen);
			if(rc == 0) break;
		}

		close(s->fd);
	}

	if(curr == NULL)
	{
		freeaddrinfo(res);
		POOL_FREE(sock_t, s);

		return NULL;
	}

	set_non_blocking(s->fd);
	freeaddrinfo(res);
  s->status = (ASIO_WRITEABLE | ASIO_READABLE);

	return s;
}

static void consume_data(chunk_list_t* container, void** data, size_t len)
{
	size_t left;
	size_t copy;

	while(len > 0)
	{
		left = bytes_left(container->writep, container->tail);

		if(left == 0)
		{
			append_chunk(container);
			left = BUFFER_SIZE;
		}

		copy = left < len ? left : len;

		memcpy(container->writep, *data, copy);

		*data += copy;
		container->writep += copy;

		len -= copy;
	}
}

static uint32_t flush_data(sock_t* s, size_t* nrp)
{
	chunk_list_t* writes = &s->writes;
	sock_chunk_t* curr;

	struct iovec iov[writes->chunks];

	iov[0].iov_base = writes->readp;
	iov[0].iov_len = writes->chunks > 1 ? BUFFER_SIZE : writes->size;

	curr = writes->head->next;

	for(int i = 1; i < writes->chunks - 1; i++)
	{
		iov[i].iov_base = curr->data;
		iov[i].iov_len = BUFFER_SIZE;

		curr = curr->next;
	}

  if(writes->chunks > 1)
	{
		iov[writes->chunks - 1].iov_base = curr->data;
		iov[writes->chunks - 1].iov_len = bytes_available(writes->writep,
			writes->tail);
	}

	return sock_writev(s->fd, iov, writes->chunks, nrp);
}

static void buffer_advance(chunk_list_t* list, void** p, size_t bytes)
{
	sock_chunk_t* curr;
	size_t left;

	while(true)
	{
		curr = list->head;
		left = bytes_left(*p, curr);

		if(left > bytes)
		{
			*p += bytes;
			return;
		}

		list->head = list->head->next;
		*p = list->head->data;

		bytes -= left;
		list->chunks--;

		POOL_FREE(sock_chunk_t, curr);
	}
}

int sock_get_fd(sock_t* s)
{
	return s->fd;
}

sock_t* sock_listen(const char* service, uint32_t backlog)
{
	sock_t* s = create_socket(NULL, service, BIND);

	if(s != NULL)
	{
		if(listen(s->fd, backlog) != 0)
		{
			POOL_FREE(sock_t, s);
			return NULL;
		}
	}

	s->status = ASIO_LISTENING;
	return s;
}

uint32_t sock_accept(sock_t* listener, sock_t** accepted)
{
	sock_t* s = POOL_ALLOC(sock_t);

	s->fd = accept(listener->fd, NULL, NULL);

	if(s->fd == -1)
	{
		POOL_FREE(sock_t, s);

		if(errno == EWOULDBLOCK || errno == EAGAIN)
			return ASIO_WOULDBLOCK;
		else
			return ASIO_ERROR;
	}

	set_non_blocking(s->fd);

	*accepted = s;

	return ASIO_SUCCESS;
}

uint32_t sock_connect(const char* host, const char* port, sock_t** connected)
{
	sock_t* s = create_socket(host, port, CONNECT);

	if(s != NULL)
	{
		*connected = s;
		return errno == EINPROGRESS ? ASIO_WOULDBLOCK : ASIO_SUCCESS;
	}

	return ASIO_ERROR;
}

size_t sock_bookmark(sock_t* sock, size_t len)
{
	size_t index = 0;
	size_t left;
	size_t forward;

	if(sock->writes.head == NULL)
	{
		append_chunk(&sock->writes);
	}
  else
	{
		//the index is the distance from the beginning of the head chunk, to the
		//current write pointer.
		index += BUFFER_SIZE * (sock->writes.chunks - 1);
		index += bytes_available(sock->writes.writep, sock->writes.tail);
	}

	while(len > 0)
	{
		left = bytes_left(sock->writes.writep, sock->writes.tail);
		forward = left < len ? left : len;

		sock->writes.writep += forward;
		len -= forward;

		if(left == forward) append_chunk(&sock->writes);

		sock->writes.size += forward;
	}

	return index;
}

void sock_bookmark_write(sock_t* sock, size_t index, void* data, size_t len)
{
	void* start;
	sock_chunk_t* curr = sock->writes.head;

	size_t copy;
	size_t left;

	while(index > BUFFER_SIZE)
	{
		curr = curr->next;
		index -=  BUFFER_SIZE;
	}

	start = curr->data + index;

	while(len > 0)
	{
		left = bytes_left(start, curr);
		copy = left < len ? left : len;

		memcpy(start, data, copy);

		data += copy;
		len -= copy;

		if(left == copy)
		{
			curr = curr->next;
			start = curr->data;
		}
	}
}

uint32_t sock_read(sock_t* sock)
{
	uint32_t rc = 0;

	size_t left;
	size_t read;

	chunk_list_t* reads = &sock->reads;

	if(reads->head == NULL)
	{
		append_chunk(reads);
		reads->readp = reads->head->data;
	}

	left = bytes_left(reads->writep, reads->head);

	while(left > 0)
	{
		rc = sock_read1(sock->fd, reads->writep, left, &read);

		if(rc & (ASIO_ERROR | ASIO_WOULDBLOCK))
			break;

		reads->writep += read;
		reads->size += read;

		left -= read;
	}

	if(rc & ASIO_WOULDBLOCK) sock->status &= ~ASIO_READABLE;

	if(left == 0)
	{
		//consumed all available space of the current chunk
		append_chunk(reads);
		reads->writep = reads->tail->data;
	}

	return rc;
}

void sock_get_data(sock_t* sock, void* dest, size_t len)
{
	size_t left;
	size_t provide;
	size_t available;

	sock_chunk_t* curr = sock->reads.head;

	while(len > 0)
	{
		left = bytes_left(sock->reads.readp, curr);
		available = sock->reads.size;

		provide = len > left ? left : len;
		provide = available > provide ? provide : available;

		memcpy(dest, sock->reads.readp, provide);

		len -= provide;
		dest += provide;

		buffer_advance(&sock->reads, &sock->reads.readp, provide);

		sock->reads.size -= provide;
	}
}

void sock_write(sock_t* sock, void* data, size_t len)
{
	chunk_list_t* writes = &sock->writes;

	if(writes->head == NULL)
	{
		append_chunk(writes);
		writes->readp = writes->head->data;
	}

	consume_data(writes, &data, len);
	writes->size += len;
}

uint32_t sock_flush(sock_t* s)
{
	size_t nrp;
	uint32_t rc;

	while(true)
	{
		rc = flush_data(s, &nrp);

		if(rc & (ASIO_ERROR | ASIO_WOULDBLOCK))
			break;

		buffer_advance(&s->writes, &s->writes.readp, nrp);

		s->writes.size -= nrp;

		if(s->writes.size == 0)
		{
			//head and tail must point to the same chunk
			POOL_FREE(sock_chunk_t, s->writes.head);

			s->writes.head = s->writes.tail = NULL;
			s->writes.readp = s->writes.writep = NULL;

			s->writes.chunks--;

			s->status |= ASIO_WRITEABLE;
			return rc;
		}
	}

	if(rc & ASIO_WOULDBLOCK) s->status &= ~ASIO_WRITEABLE;

	return rc;
}

size_t sock_read_buffer_size(sock_t* s)
{
	return s->reads.size;
}

bool sock_close(sock_t* sock)
{
	if(sock->writes.head != NULL) return false;

	close(sock->fd);
	POOL_FREE(sock_t, sock);

	return true;
}

#endif
