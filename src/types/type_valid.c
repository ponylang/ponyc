#include "type_valid.h"
#include "typechecker.h"

/**
 * This checks that all explicit types are valid.
 */
bool type_valid(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_TYPE:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      // FIX: check for trait conflicts and missing implementations
      break;

    case TK_NOMINAL:
      // FIX: make sure typeargs are within constraints
      // FIX: juggle IDSEQ and CAP
      // FIX: transform to a different ast node?
      break;

    case TK_ASSIGN:
      // FIX: check for syntactic sugar for update
      break;

    case TK_RECOVER:
      // FIX: remove things from following scope
      break;

    case TK_FOR:
      // FIX: syntactic sugar for a while loop
      break;

    case TK_REF:
      // FIX:
      if(is_type_id(ast_name(ast_child(ast))))
      {

      } else {

      }
      break;

    default: {}
  }

  return true;
}
