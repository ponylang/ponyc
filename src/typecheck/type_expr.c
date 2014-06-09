#include "type_expr.h"
#include "typechecker.h"

/**
 * This checks the type of all expressions.
 */
bool type_expr(ast_t* ast, int verbose)
{
  switch(ast_id(ast))
  {
    case TK_FVAR:
    case TK_FVAL:
      // TODO: initializer type must match declared type
      // if no declared type, get the type from the initializer
      break;

    case TK_NEW:
      // TODO: check that the object is fully initialised
      // TODO: if ?, check that we might actually error
      // TODO: if not ?, check that we can't error
      // TODO: can only return this
      break;

    case TK_BE:
      // TODO: can only return None
      // TODO: check that we can't error
      break;

    case TK_FUN:
      // TODO: body type must match return types
      // TODO: if ?, check that we might actually error
      // TODO: if not ?, check that we can't error
      break;

    case TK_SEQ:
      // TODO: type is the type of the last expr
      break;

    case TK_CONTINUE:
      // TODO: check we are in a loop
      break;

    case TK_BREAK:
      // TODO: check we are in a loop
      // TODO: type of loop is unioned with type of expr
      break;

    case TK_RETURN:
      // TODO: type of expr must match type of method
      break;

    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
    case TK_PLUS:
      // TODO: matching number types
      break;

    case TK_MINUS:
      // TODO: may be unary or binary
      break;

    case TK_LSHIFT:
    case TK_RSHIFT:
      // TODO: check bit length
      break;

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      // TODO: ordered
      break;

    case TK_EQ:
    case TK_NE:
      // TODO: comparable
      break;

    case TK_IS:
    case TK_ISNT:
      // TODO:
      break;

    case TK_AND:
    case TK_XOR:
    case TK_OR:
      // TODO:
      break;

    case TK_NOT:
      // TODO:
      break;

    case TK_ASSIGN:
      // TODO:
      break;

    case TK_CONSUME:
      // TODO: path handling
      break;

    case TK_DOT:
      // TODO: tuple, field or method
      break;

    case TK_BANG:
      // TODO: syntactic sugar for partial application
      break;

    case TK_QUALIFY:
      // TODO: find the method
      // TODO: make sure typeargs are within constraints
      break;

    case TK_CALL:
      // TODO: find the method
      // TODO: check arguments match
      // TODO: type is the type of the method call return
      // TODO: mark enclosing as "may error" if we might error
      break;

    case TK_IF:
      // TODO: first must be a Bool
      // TODO: type is union of second and third
      break;

    case TK_WHILE:
      // TODO: first must be a Bool
      // TODO: type is union of second and third, plus any break
      break;

    case TK_REPEAT:
      // TODO: type is the type of the first
      // TODO: second must be a Bool
      break;

    case TK_TRY:
      // TODO: type is the union of first and second
      // TODO: check that the first is marked as "may error"
      break;

    case TK_TUPLE:
      // TODO: create a tuple type for the object
      break;

    case TK_ARRAY:
      // TODO: determine our type by looking at every expr in the array
      break;

    case TK_OBJECT:
      // TODO: create a structural type for the object
      // TODO: make sure it fulfills any traits it claims to have
      break;

    case TK_REF:
    {
      // everything we reference must be in scope
      const char* name = ast_name(ast_child(ast));
      ast_t* ref = ast_get(ast, name);

      if(ref == NULL)
      {
        ast_error(ast, "can't find definition of '%s'", name);
        return false;
      }

      if(!is_type_id(name))
      {
        // variables must be declared before they are used
        if((ast_line(ref) > ast_line(ast)) ||
           ((ast_line(ref) == ast_line(ast)) &&
            (ast_pos(ref) > ast_pos(ast))))
        {
          ast_error(ast, "definition of '%s' appears after use", name);
          ast_error(ref, "definition of '%s' appears here", name);
          return false;
        }
      }

      // TODO: get the type?
      break;
    }

    default: {}
  }

  return true;
}
