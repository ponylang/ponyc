#if 0

#ifndef dist_proto_h
#define dist_proto_h

#include <stdbool.h>
#include <stddef.h>
#include <platform.h>

#include "../asio/sock.h"

PONY_EXTERN_C_BEGIN

enum
{
  CONTROL_NODE_ID           = 0x0001,
  CONTROL_NODE_ACCEPTED     = 0x0002,
  CONTROL_NODE_DELEGATE     = 0x0004,
  CONTROL_NODE_PEER_ADDR    = 0x0008,
  CONTROL_NODE_ADVERTISE    = 0x0010,
  CONTROL_CORE_REGISTER     = 0x0020,
  CONTROL_ACTOR_MIGRATION   = 0x0040,
  CONTROL_CLUSTER_TERMINATE = 0x0080,
  APP_ACTOR_MSG             = 0x0100,
  APP_CYCLE_UNBLOCK         = 0x0200,
  APP_CYCLE_BLOCK           = 0x0400,
  APP_CYCLE_CONFIRM         = 0x0800,
  APP_ACTOR_RC              = 0x1000,

  CONTROL_DIST_QUIESCENCE   = (1 << 14),
  PROTO_NOP                 = (1 << 15)
};

/** Opaque definition of a protocol.
 *
 */
typedef struct proto_t proto_t;

/** Receive a message from a socket.
 *
 *  Returns the TYPE of a received message, or PROTO_NOP if receiving the
 *  message was not completed yet.
 */
uint16_t proto_receive(proto_t** p, sock_t* s);

/** Send a message to some remote peer that has no message body.
 *
 */
uint32_t proto_send(uint16_t header, sock_t* s);

/** Start a protocol phase.
 *
 */
void proto_start(proto_t** p, uint16_t header, sock_t* s);

/** Record a larger message for being sent to some remote peer.
 *
 */
void proto_record(proto_t* p, void* data, size_t len);

/** Flush a message to the network.
 *
 */
uint32_t proto_finish(proto_t* p);

/** Reset the internal state of a protocol.
 *
 */
void proto_reset(proto_t* p);

PONY_EXTERN_C_END

#endif

#endif
