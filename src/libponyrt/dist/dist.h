#ifndef dist_dist_h
#define dist_dist_h

#include "../actor/messageq.h"
#include <stdbool.h>
#include <stdint.h>
#include <platform.h>

PONY_EXTERN_C_BEGIN

/** Creates an instance of the Distribution Actor
 *
 *  Every node within a cluster of Ponies runs a single Distribution Actor.
 *  A unique node must be determined as master node. The master discovers nodes
 *  within a network (TODO). Slave nodes may also be added manually.
 */
void dist_create(char* service, size_t leaf_size, bool master);

/** Manually connects this node to a master.
 *
 *  The master node either accepts this node as a direct child or delegates this
 *  node to a slot towards a path down the tree network topology. This process
 *  is repeated by intermediate slave nodes until a free slot is found.
 */
void dist_join(char* host, char* service);

/** Tells the Distribution Actor to send a message to a remote node.
 *
 *  Remote messages are actor application messages. The target node is known and
 *  determined by the memory address the receiving actor had on this machine.
 */
void dist_delegate(pony_msg_t* msg);

/** Triggers a Distributed Termination protocol.
 *
 *  Used to detect distributed quiescence. Strongly depends on causal message
 *  delivery. The Distribution Actor unsubscribes its I/O events if distributed
 *  quiesence can be established, otherwise this call has no effect.
 */
void dist_finish();

PONY_EXTERN_C_END

#endif
