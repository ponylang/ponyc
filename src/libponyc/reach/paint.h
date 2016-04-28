#ifndef PAINT_H
#define PAINT_H

#include <platform.h>
#include "reach.h"

PONY_EXTERN_C_BEGIN

/** Perform colour allocation for vtable index assignment on the given set of
 * reachable types. The resulting vtable indices and sizes are written back
 * into the given set.
 */
void paint(reach_types_t* types);


PONY_EXTERN_C_END

#endif
