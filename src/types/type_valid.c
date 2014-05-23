#include "type_valid.h"
#include "typechecker.h"

/**
 * This checks that all explicit types are valid.
 */
bool type_valid(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_TYPE:
    case TK_TRAIT:
    case TK_CLASS:
    case TK_ACTOR:
      // TODO: check for trait conflicts and missing implementations
      break;

    case TK_NOMINAL:
      // TODO: make sure typeargs are within constraints
      // TODO: juggle IDSEQ and CAP
      // TODO: transform to a different ast node?
      break;

    case TK_ASSIGN:
      // TODO: check for syntactic sugar for update
      break;

    case TK_RECOVER:
      // TODO: remove things from following scope
      break;

    case TK_FOR:
      // TODO: syntactic sugar for a while loop
      break;

    case TK_REF:
      // TODO:
      if(is_type_id(ast_name(ast_child(ast))))
      {

      } else {

      }
      break;

    default: {}
  }

  return true;
}
