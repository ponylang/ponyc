#include "proto.h"
#include "../asio/asio.h"
#include "../mem/pool.h"

#include <string.h>
#include <stdio.h>

#ifndef PLATFORM_IS_WINDOWS

enum
{
	MASK  = 0xF000,
  MAGIC = 0xA000,
  TYPE  = 0x0FFF
};

typedef struct prefix_t
{
	uint16_t header;
	uint32_t length;
} prefix_t;

struct proto_t
{
	prefix_t prefix;
  sock_t* target;
  size_t bookmark;
	bool header_complete;
};

static proto_t* init()
{
	proto_t* p = POOL_ALLOC(proto_t);

	proto_reset(p);

	return p;
}

static void read_header(proto_t* p, sock_t* s)
{
 	sock_get_data(s, &p->prefix, sizeof(p->prefix.header));
	sock_get_data(s, &p->prefix.length, sizeof(p->prefix.length));

 	p->header_complete = true;
}

static uint16_t get_message_type(proto_t* p, sock_t* s)
{
	uint16_t type;

	if((p->prefix.header & MASK) != MAGIC)
	  return PROTO_NOP;

	type = (p->prefix.header & TYPE);

	return type;
}

static bool check_for_message(proto_t* p, sock_t* s)
{
	if(p == NULL) return false;

	size_t avail = sock_read_buffer_size(s);
	size_t req = sizeof(p->prefix.header) + sizeof(p->prefix.length);

	if(avail >= req && !p->header_complete) read_header(p, s);

  if(avail >= (req + p->prefix.length)) return true;

	return false;
}

uint16_t proto_receive(proto_t** p, sock_t* s)
{
	proto_t* self = *p;

  uint32_t rc;

	if(self == NULL)
	{
		self = init();
		*p = self;
	}

	while(true)
  {
	  rc = sock_read(s);

		if(check_for_message(self, s))
			return get_message_type(self, s);

		if(rc & (ASIO_ERROR | ASIO_WOULDBLOCK))
			break;
	}

	return PROTO_NOP;
}

uint32_t proto_send(uint16_t header, sock_t* s)
{
	prefix_t prefix;

	prefix.header = (header | MAGIC);
	prefix.length = 0;

  sock_write(s, &prefix, sizeof(prefix.header));
	sock_write(s, &prefix.length, sizeof(prefix.length));

	return sock_flush(s);
}

void proto_start(proto_t** p, uint16_t header, sock_t* s)
{
	proto_t* self = *p;

	if(self == NULL)
	{
		self = init();
		*p = self;
	}

	self->prefix.header = (header | MAGIC);
	self->prefix.length = 0;
  self->target = s;

	sock_write(s, &self->prefix.header, sizeof(self->prefix.header));
	self->bookmark = sock_bookmark(s, sizeof(self->prefix.length));
}

void proto_record(proto_t* p, void* data, size_t len)
{
	sock_write(p->target, data, len);
	p->prefix.length += len;
}

uint32_t proto_finish(proto_t* p)
{
	if(p == NULL || p->target == NULL)
		return 0;

	sock_bookmark_write(p->target, p->bookmark, &p->prefix.length,
		sizeof(p->prefix.length));

	return sock_flush(p->target);
}

void proto_reset(proto_t* p)
{
	p->prefix.header = 0;
	p->prefix.length = 0;
	p->target = NULL;
	p->bookmark = 0;

	p->header_complete = false;
}

#endif
