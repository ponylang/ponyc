#include "dartsrc.h"
#include <stdio.h>
#include <string.h>


static void dart_node(FILE* fp, ast_t* ast);
static void dart_class(FILE* fp, ast_t* ast);
static void dart_method(FILE* fp, ast_t* ast);
static void dart_expr(FILE* fp, ast_t* ast);


static void dart_node(FILE* fp, ast_t* ast)
{
  if (ast == NULL) return;

  printf("dart_node top, ast=\n");
  //ast_fprint(stderr, ast, 80); // actually does print, far too much though!
 
  switch (ast_id(ast))
  {
    case TK_MODULE:
      for (ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p)) {
        dart_node(fp, p);
      }
      break;

    case TK_ACTOR:
      dart_class(fp, ast);
      break;

    case TK_NEW:
    case TK_FUN:
      dart_method(fp, ast);
      break;

    case TK_SEQ:
      printf("dart_node(): TK_SEQ\n");
      for (ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p)) {
        fprintf(fp, "    ");
        dart_node(fp, p);
        fprintf(fp, ";\n");
      }
      break;

    default:
      printf("dart_node default: call dart_expr()?\n");
      dart_expr(fp, ast);
      printf("dart_node default: back from dart_expr()?\n");
      break;
  }
}

static void dart_class(FILE* fp, ast_t* ast)
{
  ast_t* id = ast_child(ast);
  fprintf(fp, "class %s {\n", ast_name(id));

  ast_t* members = ast_childidx(ast, 4);
  for (ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p)) {
    dart_node(fp, p);
  }

  fprintf(fp, "}\n");
}

static void dart_method(FILE* fp, ast_t* ast)
{
  ast_t* id = ast_childidx(ast, 1);
  ast_t* params = ast_childidx(ast, 2);
  ast_t* body = ast_childidx(ast, 4);

  if (ast_id(ast) == TK_NEW)
  {
    ast_t* class_ast = ast_parent(ast_parent(ast));
    ast_t* class_id = ast_child(class_ast);
    fprintf(fp, "  %s(", ast_name(class_id));
  } else {
    fprintf(fp, "  %s(", ast_name(id));
  }

  for (ast_t* p = ast_child(params); p != NULL; p = ast_sibling(p))
  {
    ast_t* param_id = ast_child(p);
    fprintf(fp, "var %s", ast_name(param_id));
    if (ast_sibling(p) != NULL)
    {
      fprintf(fp, ", ");
    }
  }

  fprintf(fp, ") {\n");

  dart_node(fp, body);

  fprintf(fp, "  }\n");
}

static void dart_expr(FILE* fp, ast_t* ast)
{
  if (ast == NULL) return;

  fprintf(stderr, "dart_expr: %s\n", ast_get_print(ast));

  switch (ast_id(ast))
  {
    case TK_CALL:
    {
      fprintf(stderr, "  TK_CALL\n");
      dart_node(fp, ast_child(ast)); // receiver

      fprintf(fp, "(");
      ast_t* args = ast_childidx(ast, 1);
      for (ast_t* p = ast_child(args); p != NULL; p = ast_sibling(p)) {
        dart_node(fp, p);
        if (ast_sibling(p) != NULL)
        {
          fprintf(fp, ", ");
        }
      }
      fprintf(fp, ")");
      break;
    }

    case TK_DOT:
      fprintf(stderr, "  TK_DOT\n");
      dart_node(fp, ast_child(ast));
      fprintf(fp, ".");
      dart_node(fp, ast_childidx(ast, 1));
      break;

    case TK_REFERENCE:
      fprintf(stderr, "  TK_REFERENCE: %s\n", ast_name(ast));
      fprintf(fp, "%s", ast_name(ast));
      break;

    case TK_ID:
      fprintf(stderr, "  TK_ID: %s\n", ast_name(ast));
      fprintf(fp, "%s", ast_name(ast));
      break;

    case TK_STRING:
      fprintf(stderr, "  TK_STRING: %s\n", ast_name(ast));
      fprintf(fp, "\"%s\"", ast_name(ast));
      break;

    case TK_PACKAGE:
      fprintf(stderr, "  TK_PACKAGE\n");
      fprintf(fp, "\"%s\"", ast_get_print(ast));
      //fprintf(fp, "\"%s\"", ast_name(ast)); // Assertion `token->id == TK_STRING || token->id == TK_ID` failed. due to call to token_string(ast->t) where ast->t is neither
      break;

    case TK_PROGRAM:
      fprintf(stderr, "  TK_PROGRAM\n");
      dart_node(fp, ast_child(ast));
      break;
      
    default:
      fprintf(stderr, "  unhandled %s\n", ast_get_print(ast)); // _name(ast_id(ast)));
      break;
  }
}


bool DartPrintModuleToFile(DartModuleRef M, const char *Filename,
                           char **ErrorMessage) {
  printf("TODO top DartPrintModuleToFile M=%p, Filename='%s'\n", M, Filename);

  FILE* fp = fopen(Filename, "w");
  if (fp == NULL)
  {

    if (ErrorMessage == NULL) {    
      printf("error in pass/dartsrc.c DartPrintModuleToFile(): could not open output file '%s'\n", Filename);
      return false;
    }
    const char *error_prefix = "error in DartPrintModuleToFile: could not open output file '";
    size_t msg_len = strlen(error_prefix) + strlen(Filename) + 3; // +3 for "'\n" and null terminator
    *ErrorMessage = (char *)malloc(msg_len);
    
    if (*ErrorMessage != NULL) {
      snprintf(*ErrorMessage, msg_len, "%s%s'", error_prefix, Filename);
    }
    
    return false;
  }
  
  dart_node(fp, M->program);
  return false;
}

void DartDisposeMessage(char *Message) {
  free(Message);
}
