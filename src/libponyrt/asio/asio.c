#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "asio.h"
#include "../mem/pool.h"

#ifndef PLATFORM_IS_WINDOWS

typedef ssize_t op_fn(int fd, const struct iovec *iov, int iovcnt);

struct asio_base_t
{
	pony_thread_id_t tid;
	asio_backend_t* backend;
	uint32_t subscriptions;
};

static asio_base_t* running_base;

/** Start an asynchronous I/O event mechanism.
 *
 *  Errors are always delegated to the owning actor of an I/O subscription and
 *  never handled within the runtime system.
 *
 *  In any case (independent of the underlying backend) only one I/O dispatcher
 *  thread will be started. Since I/O events are subscribed by actors, we do not
 *  need to maintain a thread pool. Instead, I/O is processed in the context of
 *  the owning actor.
 */
static void start()
{
  asio_base_t* new_base = POOL_ALLOC(asio_base_t);
	memset((void*)new_base, 0, sizeof(asio_base_t));

	new_base->backend = asio_backend_init();

	asio_base_t* existing = NULL;

	if(__pony_atomic_compare_exchange_n(&running_base, &existing,
		new_base, false, PONY_ATOMIC_RELAXED, PONY_ATOMIC_RELAXED, intptr_t))
	{
		if(!pony_thread_create(&running_base->tid, asio_backend_dispatch,
			running_base->backend))
		  exit(EXIT_FAILURE);

  	pony_thread_detach(running_base->tid);
	}
	else
	{
		asio_backend_terminate(new_base->backend);
		POOL_FREE(asio_base_t, new_base);
	}
}

/** Wrapper for writev and readv.
 */
static uint32_t exec(op_fn* fn, intptr_t fd, struct iovec* iov, size_t chunks,
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

asio_backend_t* asio_get_backend()
{
	if(running_base == NULL) start();
	return running_base->backend;
}

bool asio_stop()
{
	if(running_base->subscriptions > 0)
		return false;

  //TODO FIX: this is not thread safe
  asio_backend_terminate(running_base->backend);

	return true;
}

uint32_t asio_writev(intptr_t fd, struct iovec* iov, size_t chunks, size_t* nrp)
{
	return exec(writev, fd, iov, chunks, nrp);
}

uint32_t asio_readv(intptr_t fd, struct iovec* iov, size_t chunks, size_t* nrp)
{
	return exec(readv, fd, iov, chunks, nrp);
}

uint32_t asio_read(intptr_t fd, void* dest, size_t len, size_t* nrp)
{
	struct iovec iov[1];

	iov[0].iov_len = len;
  iov[0].iov_base = dest;

	return asio_readv(fd, iov, 1, nrp);
}

#endif
