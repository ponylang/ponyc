#include "parserapi.h"
#include <stdlib.h>
#include <assert.h>

static token_id current(parser_t* parser)
{
  return parser->t->id;
}

ast_t* consume(parser_t* parser)
{
  ast_t* ast = ast_token(parser->t);
  parser->t = lexer_next(parser->lexer);
  return ast;
}

void insert(parser_t* parser, token_id id, ast_t* ast)
{
  ast_t* child = ast_new(parser->t, id);
  ast_add(ast, child);
}

bool look(parser_t* parser, const token_id* id)
{
  while(*id != TK_NONE)
  {
    if(current(parser) == *id)
      return true;

    id++;
  }

  return false;
}

bool accept(parser_t* parser, const token_id* id, ast_t* ast)
{
  if(!look(parser, id))
    return false;

  if(ast != NULL)
  {
    ast_t* child = ast_token(parser->t);
    ast_add(ast, child);
  } else {
    token_free(parser->t);
  }

  parser->t = lexer_next(parser->lexer);
  return true;
}

ast_t* rulealt(parser_t* parser, const rule_t* alt, ast_t* ast, bool opt)
{
  while((*alt) != NULL)
  {
    ast_t* child = (*alt)(parser, opt);

    if(child != NULL)
    {
      if(ast != NULL)
        ast_add(ast, child);

      return child;
    }

    alt++;
  }

  return NULL;
}

ast_t* parse(source_t* source, rule_t start)
{
  // open the lexer
  lexer_t* lexer = lexer_open(source);

  if(lexer == NULL)
    return NULL;

  // create a parser and attach the lexer
  parser_t* parser = calloc(1, sizeof(parser_t));
  parser->source = source;
  parser->lexer = lexer;
  parser->t = lexer_next(lexer);

  ast_t* ast = start(parser, false);

  if(ast != NULL)
  {
    ast_reverse(ast);
    assert(ast_data(ast) == NULL);
    ast_setdata(ast, source);
  }

  lexer_close(lexer);
  token_free(parser->t);
  free(parser);

  return ast;
}

ast_t* bindop(parser_t* parser, prec_t prec, assoc_t assoc, ast_t* ast,
  const rule_t* alt)
{
  ast_t* next;
  int oprec = INT_MAX;

  while((next = rulealt(parser, alt, NULL, true)) != NULL)
  {
    token_id id = ast_id(next);
    int nprec = prec(id);
    bool right = assoc(id);

    while(true)
    {
      if((nprec > oprec) || (right && (nprec == oprec)) )
      {
        // (oldop ast.1 (newop ast.2 ...))
        ast_append(next, ast_pop(ast));
        ast_add(ast, next);
        ast = next;
        break;
      } else if(ast_parent(ast) == NULL) {
        // (newop ast ...)
        ast_append(next, ast);
        ast = next;
        break;
      } else {
        // try the parent node
        ast = ast_parent(ast);
        oprec = prec(ast_id(ast));
      }
    }

    oprec = nprec;
  }

  while(ast_parent(ast) != NULL)
    ast = ast_parent(ast);

  return ast;
}

void syntax_error(parser_t* parser, const char* func, int line)
{
  if(!parser->t->error)
  {
    error(parser->source, parser->t->line, parser->t->pos,
      "syntax error (%s, %d)", func, line);

    parser->t->error = true;
  }
}

void scope(ast_t* ast)
{
  ast_t* child = ast_child(ast);

  if(child == NULL)
  {
    ast_scope(ast);
  } else {
    ast_t* next = ast_sibling(child);

    while(next != NULL)
    {
      child = next;
      next = ast_sibling(child);
    }

    ast_scope(child);
  }
}
