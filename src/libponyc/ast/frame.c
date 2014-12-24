#include "frame.h"
#include "ast.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

static bool push_frame(typecheck_t* t)
{
  typecheck_frame_t* f = POOL_ALLOC(typecheck_frame_t);

  if(t->frame != NULL)
  {
    memcpy(f, t->frame, sizeof(typecheck_frame_t));
    f->prev = t->frame;
  } else {
    memset(f, 0, sizeof(typecheck_frame_t));
  }

  t->frame = f;
  return true;
}

bool frame_push(typecheck_t* t, ast_t* ast)
{
  bool pop = false;

  if(ast == NULL)
  {
    typecheck_frame_t* prev = t->frame;

    pop = push_frame(t);
    memset(t->frame, 0, sizeof(typecheck_frame_t));
    t->frame->prev = prev;

    return pop;
  }

  switch(ast_id(ast))
  {
    case TK_PACKAGE:
      // Can occur in a module as a result of a use expression.
      pop = push_frame(t);
      t->frame->package = ast;
      t->frame->module = NULL;
      break;

    case TK_MODULE:
      pop = push_frame(t);
      t->frame->module = ast;
      break;

    case TK_INTERFACE:
    case TK_TRAIT:
    case TK_PRIMITIVE:
    case TK_CLASS:
    case TK_ACTOR:
      pop = push_frame(t);
      t->frame->type = ast;
      break;

    case TK_NEW:
    case TK_BE:
    case TK_FUN:
      pop = push_frame(t);
      t->frame->method = ast;
      break;

    case TK_TYPEARGS:
      pop = push_frame(t);
      t->frame->method_type = NULL;
      t->frame->ffi_type = NULL;
      t->frame->local_type = NULL;
      break;

    case TK_CASE:
      pop = push_frame(t);
      t->frame->the_case = ast;
      break;

    case TK_WHILE:
    case TK_REPEAT:
      pop = push_frame(t);
      t->frame->loop = ast;
      break;

    case TK_TRY:
    case TK_TRY2:
      pop = push_frame(t);
      t->frame->try_expr = ast;
      break;

    case TK_RECOVER:
      pop = push_frame(t);
      t->frame->recover = ast;
      break;

    default:
    {
      ast_t* parent = ast_parent(ast);

      switch(ast_id(parent))
      {
        case TK_TYPEPARAM:
          if(ast_childidx(parent, 1) == ast)
          {
            pop = push_frame(t);
            t->frame->constraint = ast;
          }
          break;

        case TK_NEW:
        case TK_BE:
        case TK_FUN:
        {
          AST_GET_CHILDREN(parent,
            cap, id, typeparams, params, result, error, body);

          if(result == ast)
          {
            pop = push_frame(t);
            t->frame->method_type = ast;
          } else if(body == ast) {
            pop = push_frame(t);
            t->frame->method_body = ast;
          }
          break;
        }

        case TK_TYPEARGS:
        {
          switch(ast_id(ast_parent(parent)))
          {
            case TK_FFIDECL:
            case TK_FFICALL:
              pop = push_frame(t);
              t->frame->ffi_type = ast;
              break;

            default: {}
          }
          break;
        }

        case TK_PARAM:
        {
          AST_GET_CHILDREN(parent, id, type, def_arg);

          if(type == ast)
          {
            pop = push_frame(t);
            t->frame->local_type = ast;
          } else if(def_arg == ast) {
            pop = push_frame(t);
            t->frame->def_arg = ast;
          }
          break;
        }

        case TK_VAR:
        case TK_LET:
          if(ast_childidx(parent, 1) == ast)
          {
            pop = push_frame(t);
            t->frame->local_type = ast;
          }
          break;

        case TK_CASE:
          if(ast_child(parent) == ast)
          {
            pop = push_frame(t);
            t->frame->pattern = ast;
          }
          break;

        case TK_AS:
        {
          AST_GET_CHILDREN(parent, expr, type);

          if(type == ast)
          {
            pop = push_frame(t);
            t->frame->as_type = ast;
          }
          break;
        }

        case TK_WHILE:
        {
          AST_GET_CHILDREN(parent, cond, body, else_clause);

          if(cond == ast)
          {
            pop = push_frame(t);
            t->frame->loop_cond = ast;
          } else if(body == ast) {
            pop = push_frame(t);
            t->frame->loop_body = ast;
          } else if(else_clause == ast) {
            pop = push_frame(t);
            t->frame->loop_else = ast;
          }
          break;
        }

        case TK_REPEAT:
        {
          AST_GET_CHILDREN(parent, body, cond, else_clause);

          if(body == ast)
          {
            pop = push_frame(t);
            t->frame->loop_body = ast;
          } else if(cond == ast) {
            pop = push_frame(t);
            t->frame->loop_cond = ast;
          } else if(else_clause == ast) {
            pop = push_frame(t);
            t->frame->loop_else = ast;
          }
          break;
        }

        default: {}
      }
      break;
    }
  }

  return pop;
}

void frame_pop(typecheck_t* t)
{
  typecheck_frame_t* f = t->frame;
  assert(f != NULL);

  t->frame = f->prev;
  POOL_FREE(typecheck_frame_t, f);
}
