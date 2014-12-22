#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stddef.h>
#include <assert.h>

#include "dist.h"
#include "proto.h"
#include "../actor/actor.h"
#include "../sched/scheduler.h"
#include "../asio/asio.h"
#include "../asio/event.h"
#include "../asio/sock.h"
#include "../ds/hash.h"
#include "../mem/pool.h"

#if 0

#define LISTEN_BACKLOG 50

enum
{
	DIST_INIT,
	DIST_JOIN,
	DIST_DELEGATE,
	DIST_IO_NOTIFY,
	DIST_FINISH
};

typedef struct init_msg_t
{
	pony_msg_t msg;
	const char* port;
	uint32_t leaf_size;
	bool master;
} init_msg_t;

typedef struct join_msg_t
{
	pony_msg_t msg;
	const char* host;
	const char* port;
} join_msg_t;

typedef struct delegate_msg_t
{
	pony_msg_t msg;
	pony_actor_t* target;
	pony_msg_t* delegate;
} delegate_msg_t;

typedef struct node_t
{
	uint32_t node_id;
	uint32_t core_count;
	sock_t* socket;

  proto_t* proto;

	bool temporary;
	bool pending;
} node_t;

typedef struct cluster_t
{
	node_t self;
	node_t parent;
	node_t** children;
	uint32_t free_slots;
	uint32_t accepted;
} cluster_t;

typedef struct dist_t
{
	pony_actor_pad_t pad;
	cluster_t cluster;

	//hash_t* proxy_map;
	//hash_t* local_map;
	//hash_t* object_map;

	bool master;
} dist_t;

static pony_actor_t* dist_actor;
static uint32_t next_node_id;

static void trace_node(void* p)
{
	node_t* child = (node_t*)p;

  pony_trace(child->socket);
	pony_trace(child->proto);
}

static void trace_dist(void* p)
{
  dist_t* self = (dist_t*)p;

	trace_node(&self->cluster.self);
	trace_node(&self->cluster.parent);

	pony_trace(self->cluster.children);

  for(uint32_t i = 0; i < self->cluster.accepted; i++)
	{
		pony_traceobject(self->cluster.children[i], trace_node);
	}
}

/*static node_t* get_next_hop(cluster_t* cluster, uint32_t node_id)
{
	return NULL;
}*/

static void join_node(dist_t* d)
{
	cluster_t* v = &d->cluster;

	uint32_t rc = 0;
  uint32_t filter = 0;
	node_t* child = 0;
  sock_t* s = 0;

  while(true)
  {
		rc = sock_accept(v->self.socket, &s);

		if((rc & (ASIO_WOULDBLOCK | ASIO_ERROR)) != 0) break;

    child = POOL_ALLOC(node_t);
		memset(child, 0, sizeof(node_t));

		child->socket = s;

		if(d->master)
		{
			next_node_id++;

			proto_start(&v->self.proto, CONTROL_NODE_ID, s);
      proto_record(v->self.proto, &next_node_id, sizeof(next_node_id));
			rc = proto_finish(v->self.proto);

			child->node_id = next_node_id;
		}

		if(v->free_slots > 0)
		{
      v->children[v->accepted] = child;
			child->temporary = false;

			rc |= proto_send(CONTROL_NODE_ACCEPTED, s);

			v->free_slots--;
			v->accepted++;
		}
		else
		{
			//node delegate
		}

		filter = ASIO_FILT_READ;

		if((rc & ASIO_WOULDBLOCK) != 0)
		{
			filter |= ASIO_FILT_WRITE;
		}
		else if(child->temporary)
		{
			sock_close(child->socket);
			POOL_FREE(node_t, child);

			continue;
		}
		else
		{
			proto_reset(v->self.proto);
		}

		asio_event_t* e = asio_event_create(
			actor_current(),
			DIST_IO_NOTIFY,
			sock_get_fd(child->socket),
			filter,
			false,
			child
			);

		asio_event_subscribe(e);
  }
}

static void handle_remote_message(node_t* n, uint16_t type)
{
	switch(type)
	{
		case CONTROL_NODE_ID:
		{
			sock_t* s = n->socket;
			sock_get_data(s, &n->node_id, sizeof(n->node_id));

			printf("[DIST] Received node id: %d\n", n->node_id);

			break;
		}

		case CONTROL_NODE_DELEGATE:
		  break;

		case CONTROL_NODE_ACCEPTED:
		  printf("[DIST] Accepted at parent!\n");
		  break;

		case CONTROL_NODE_PEER_ADDR:
 		  break;

		case CONTROL_CORE_REGISTER:
		  break;
	}
}

static void dispatch(pony_actor_t* self, pony_msg_t* msg)
{
	dist_t* d = (dist_t*)self;

	switch(msg->id)
	{
		case DIST_INIT:
		{
			init_msg_t* m = (init_msg_t*)msg;

	    d->cluster.children = (node_t**)pony_alloc(
				sizeof(node_t*) * m->leaf_size);

		  d->cluster.free_slots = m->leaf_size;
		  d->master = m->master;
		  d->cluster.self.socket = sock_listen(m->port, LISTEN_BACKLOG);

	    asio_event_t* e = asio_event_create(
				actor_current(),
				DIST_IO_NOTIFY,
				sock_get_fd(d->cluster.self.socket),
				ASIO_FILT_READ,
				false,
				&d->cluster.self
				);

	    asio_event_subscribe(e);
		  break;
		}

		case DIST_JOIN:
		{
			join_msg_t* m = (join_msg_t*)msg;

			d->cluster.parent.temporary = false;
			d->cluster.parent.pending = true;

		  uint32_t rc = sock_connect(m->host, m->port,
				&d->cluster.parent.socket);

			uint32_t flt = ASIO_FILT_READ;

		  if(rc == ASIO_WOULDBLOCK)
				flt |= ASIO_FILT_WRITE;

      asio_event_t* e = asio_event_create(
				actor_current(),
				sock_get_fd(d->cluster.parent.socket),
				flt,
			  DIST_IO_NOTIFY,
			  false,
				&d->cluster.parent
		  	);

			asio_event_subscribe(e);
		  break;
		}

		case DIST_DELEGATE:
		{
			delegate_msg_t* m = (delegate_msg_t*)msg;

			pony_gc_recv();
			pony_traceactor(m->target);
			pony_recv_done();

		  //TODO

			pool_free(m->msg.size, m);
		  break;
		}

		case DIST_IO_NOTIFY:
		{
			asio_msg_t* m = (asio_msg_t*)msg;
      node_t* n = (node_t*)m->event->udata;

			if((m->flags & (ASIO_ERROR | ASIO_PEER_SHUTDOWN |
				ASIO_CLOSED_UNEXPECTEDLY)) != 0)
			{
      	sock_close(n->socket); // TODO error handling / termination

				POOL_FREE(node_t, n);
				asio_event_unsubscribe(m->event);
				break;
			}

      if((m->flags & ASIO_READABLE) != 0)
      {
      	if(n == &d->cluster.self)
				{
      		join_node(d);
				}
      	else
      	{
					uint16_t proto = proto_receive(&n->proto, n->socket);

				  if(proto != PROTO_NOP)
					{
						assert(m->event->owner == self);
						m->flags = ASIO_READABLE;
						asio_event_send(m->event, m->flags);

          	handle_remote_message(n, proto);
						proto_reset(n->proto);
					}
      	}
      }

      if((m->flags & ASIO_WRITABLE) != 0)
			{
				if(proto_finish(n->proto) == ASIO_SUCCESS)
				{
					if(n->temporary)
					{
						asio_event_unsubscribe(m->event);
						POOL_FREE(node_t, n);
					}

					proto_reset(n->proto);
				}
			}

      break;
		}

		case DIST_FINISH:
		  //TODO
		  break;
	}
}

static pony_type_t type =
{
	0,
	sizeof(dist_t),
	0,
	0,
	trace_dist,
	NULL,
	NULL,
	dispatch,
	NULL
};

void dist_create(char* port, uint32_t leaf_size, bool master)
{
	init_msg_t* m = (init_msg_t*)pony_alloc_msg(0, DIST_INIT);
	m->port = port;
	m->leaf_size = leaf_size;
	m->master = master;

	dist_actor = pony_create(&type);
	actor_setsystem(dist_actor);
	pony_sendv(dist_actor, &m->msg);
}

void dist_join(char* host, char* port)
{
	join_msg_t* m = (join_msg_t*)pony_alloc_msg(0, DIST_JOIN);
	m->host = host;
	m->port = port;

	pony_sendv(dist_actor, &m->msg);
}

void dist_delegate(pony_msg_t* msg)
{
	pony_msg_t* clone = pool_alloc(msg->size);
	size_t size = pool_size(msg->size);
	memcpy(clone, msg, size);

	delegate_msg_t* m = (delegate_msg_t*)pony_alloc_msg(0, DIST_DELEGATE);
	m->target = actor_current();
	m->delegate = clone;

	pony_gc_send();
	pony_traceactor(m->target);
	pony_send_done();

	pony_sendv(dist_actor, &m->msg);
}

void dist_finish()
{
	pony_send(dist_actor, DIST_FINISH);
}

#endif
