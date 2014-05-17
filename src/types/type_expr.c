#include "type_expr.h"

/**
 * This checks the type of all expressions.
 */
bool type_expr(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_FVAR:
    case TK_FVAL:
      // FIX: initializer type must match declared type
      break;

    case TK_NEW:
      // FIX: check that the object is fully initialised
      // FIX: if ?, check that we might actually error
      // FIX: if not ?, check that we can't error
      // FIX: can only return this
      break;

    case TK_BE:
      // FIX: can only return None
      // FIX: check that we can't error
      break;

    case TK_FUN:
      // FIX: body type must match return types
      // FIX: if ?, check that we might actually error
      // FIX: if not ?, check that we can't error
      break;

    case TK_SEQ:
      // FIX: type is the type of the last expr
      break;

    case TK_CONTINUE:
      // FIX: check we are in a loop
      break;

    case TK_BREAK:
      // FIX: check we are in a loop
      // FIX: type of loop is unioned with type of expr
      break;

    case TK_RETURN:
      // FIX: type of expr must match type of method
      break;

    case TK_MULTIPLY:
    case TK_DIVIDE:
    case TK_MOD:
    case TK_PLUS:
      // FIX: matching number types
      break;

    case TK_MINUS:
      // FIX: may be unary or binary
      break;

    case TK_LSHIFT:
    case TK_RSHIFT:
      // FIX: check bit length
      break;

    case TK_LT:
    case TK_LE:
    case TK_GE:
    case TK_GT:
      // FIX: ordered
      break;

    case TK_EQ:
    case TK_NE:
      // FIX: comparable
      break;

    case TK_IS:
    case TK_ISNT:
      // FIX:
      break;

    case TK_AND:
    case TK_XOR:
    case TK_OR:
      // FIX:
      break;

    case TK_NOT:
      // FIX:
      break;

    case TK_ASSIGN:
      // FIX:
      break;

    case TK_CONSUME:
      // FIX: path handling
      break;

    case TK_DOT:
      // FIX: tuple, field or method
      break;

    case TK_BANG:
      // FIX: syntactic sugar for partial application
      break;

    case TK_QUALIFY:
      // FIX: find the method
      // FIX: make sure typeargs are within constraints
      break;

    case TK_CALL:
      // FIX: find the method
      // FIX: check arguments match
      // FIX: type is the type of the method call return
      // FIX: mark enclosing as "may error" if we might error
      break;

    case TK_IF:
      // FIX: first must be a Bool
      // FIX: type is union of second and third
      break;

    case TK_WHILE:
      // FIX: first must be a Bool
      // FIX: type is union of second and third, plus any break
      break;

    case TK_REPEAT:
      // FIX: type is the type of the first
      // FIX: second must be a Bool
      break;

    case TK_TRY:
      // FIX: type is the union of first and second
      // FIX: check that the first is marked as "may error"
      break;

    case TK_TUPLE:
      // FIX: create a tuple type for the object
      break;

    case TK_ARRAY:
      // FIX: determine our type by looking at every expr in the array
      break;

    case TK_OBJECT:
      // FIX: create a structural type for the object
      // FIX: make sure it fulfills any traits it claims to have
      break;

    case TK_REF:
      // FIX: must be declared before it was used
      // FIX: get the type
      break;

    default: {}
  }

  return true;
}
