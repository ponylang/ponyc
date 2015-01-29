#ifndef PAINT_H
#define PAINT_H

#include <platform.h>
#include "reach.h"

PONY_EXTERN_C_BEGIN

/** The painter interface is responsible for performing colour allocation for
 * vtable index assignment.
 *
 * We start with a list of all the used concrete types (actors, classes and
 * data type-values). We do not care about type parameters on these types. If
 * type parameters on methods are added later then we will care about those.
 *
 * For now we accept an AST and walk it to find all the concrete type
 * definitions. Later this may change to use accepting a list of definition
 * ASTs.
 *
 * Once we have performed colouring the painter can be interrogated to find the
 * index assigned to any given method name.
 */

//typedef struct painter_t painter_t;

/** The various performance options are stored within a colour_opt_t. These can
 * be retrieved and set. Sensible defaults will be provided, the ability to
 * override this is mainly provided for the sake of testing.
 */
//typedef struct colour_opt_t
//{
//  int none_yet;
//} colour_opt_t;

/// Create a painter to perform colouring
//painter_t* painter_create();

/** Get the options struct for the given painter.
 * The performance options in the returned struct may be altered, this must
 * occur before the call to painter_colour().
 * The struct returned is the actual one within the painter. It should not be
 * freed and changes to it are automatically picked up by the painter.
 */
//colour_opt_t* painter_get_options(painter_t* painter);

/** Perform colour assignment for the concrete types in the given AST.
 * The whole AST must be provided in one go. Successive calls will not add to
 * the existing colouring.
 */
//void painter_colour(painter_t* painter, ast_t* typedefs);

/** Report the vtable index assigned to the given method name.
 * Returns the index of the method or <0 if the method name is not defined on
 * any of the provided types.
 */
//int painter_get_colour(painter_t* painter, const char* method_name);

/** Report the number of entries needed in the vtable for the specified type.
 * TODO: For now we do a linear lookup to find the specified definition, we
 * should use a hash table instead.
 */
//int painter_get_vtable_size(painter_t* painter, ast_t* type);

/// Free the given painter
//void painter_free(painter_t* painter);


void paint(reachable_types_t* types);


PONY_EXTERN_C_END

#endif
