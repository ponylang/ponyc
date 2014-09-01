#include "ast.h"
#include "builder.h"
#include "lexer.h"
#include "parserapi.h"
#include "symtab.h"
#include "../ds/stringtab.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdio.h>

typedef enum ast_token_id
{
  AT_LPAREN,
  AT_RPAREN,
  AT_LSQUARE,
  AT_RSQUARE,
  AT_LBRACE,
  AT_RBRACE,
  AT_ID,      // Pony name
  AT_TOKEN,   // Any other Pony token
  AT_EOF,
  AT_ERROR
} ast_token_id;


typedef struct builder_ref_t
{
  ast_t* node;        // Referencing node
  char const* name;   // Reference name
  char const* symtab; // Name to add to symtab. NULL => set AST data
  struct builder_ref_t* next;
} builder_ref_t;


// Since we can add new sub-trees after creation we need to store multiple
// sources. We use a simple linked list
typedef struct builder_src_t
{
  source_t* source;
  struct builder_src_t* next;
} builder_src_t;


/* Note: We build up an AST and then hand out pointers to it. We also keep hold
 * of the tree so we can delete it later. However, if someone takes a pointer
 * to the root of the tree and performs an ast_replace, then we end up with the
 * old tree, which will have been deleted. To get round this we add an extra
 * dummy node to the root of the tree we build. We keep this node hidden, only
 * giving out pointers to nodes further down the tree. If the "root" is
 * replaced our dummy will be updated to contain the new tree automatically.
 */
typedef struct builder_t
{
  builder_src_t* sources;
  symtab_t* defs;
  ast_t* dummy_root;
} builder_t;


typedef struct build_parser_t
{
  lexer_t* lexer;
  source_t* source;
  token_t* token;
  symtab_t* defs;
  builder_ref_t* refs;
  ast_token_id id;
  int line;
  int pos;
  bool have_token;
  bool had_error;
} build_parser_t;


static ast_t* get_nodes(build_parser_t* builder, ast_token_id terminator);


// Report an error
static void build_error(build_parser_t* builder, const char* fmt, ...)
{
  assert(builder != NULL);
  assert(builder->source != NULL);

  if(builder->had_error)
    return;

  va_list ap;
  va_start(ap, fmt);
  errorv(builder->source, builder->line, builder->pos, fmt, ap);
  va_end(ap);

  builder->had_error = true;
}


// Add a sub-tree name definition
static bool add_subtree_name(build_parser_t* builder, const char* name,
  ast_t* subtree)
{
  assert(builder != NULL);
  assert(builder->defs != NULL);

  if(!symtab_add(builder->defs, name, (void*)subtree))
  {
    build_error(builder, "Multiple {def %s} attributes", name);
    return false;
  }

  return true;
}


// Add a reference to a sub-tree
static void add_subtree_ref(build_parser_t* builder, ast_t* node,
  const char* name, const char* symtab)
{
  assert(builder != NULL);

  builder_ref_t* newref = (builder_ref_t*)malloc(sizeof(builder_ref_t));
  newref->node = node;
  newref->name = name;
  newref->symtab = symtab;
  newref->next = builder->refs;
  builder->refs = newref;
}


// Process all our sub-tree references
static bool process_refs(build_parser_t* builder)
{
  assert(builder != NULL);
  assert(builder->defs != NULL);

  for(builder_ref_t* p = builder->refs; p != NULL; p = p->next)
  {
    assert(p->name != NULL);
    assert(p->node != NULL);

    ast_t* subtree = (ast_t*)symtab_get(builder->defs, p->name);

    if(subtree == NULL)
    {
      build_error(builder, "Attribute {def %s} not found", p->name);
      return false;
    }

    if(p->symtab == NULL)
    {
      // Set node data
      ast_setdata(p->node, subtree);
    }
    else
    {
      // Add subtree to node's symtab
      symtab_t* symtab = ast_get_symtab(p->node);
      assert(symtab != NULL);
      if(!symtab_add(symtab, p->symtab, subtree))
      {
        build_error(builder, "Duplicate name %s in symbol table", p->name);
        return false;
      }
    }
  }

  return true;
}


// Get the next token ready for when we need it
static void get_next_token(build_parser_t* builder)
{
  assert(builder != NULL);

  if(builder->have_token)
    return;

  if(builder->token != NULL)
    token_free(builder->token);

  builder->token = lexer_next(builder->lexer);
  ast_token_id id;

  switch(token_get_id(builder->token))
  {
    case TK_LPAREN_NEW:
    case TK_LPAREN:     id = AT_LPAREN;  break;
    case TK_RPAREN:     id = AT_RPAREN;  break;
    case TK_LSQUARE_NEW:
    case TK_LSQUARE:    id = AT_LSQUARE; break;
    case TK_RSQUARE:    id = AT_RSQUARE; break;
    case TK_LBRACE:     id = AT_LBRACE;  break;
    case TK_RBRACE:     id = AT_RBRACE;  break;
    case TK_EOF:        id = AT_EOF;     break;
    case TK_LEX_ERROR:  id = AT_ERROR;   break;
    case TK_ID:         id = AT_ID;      break;
    default:            id = AT_TOKEN;   break;
  }

  //printf("Got token %d -> %d\n", token_get_id(builder->token), id);
  builder->id = id;
  builder->have_token = true;
  builder->line = token_line_number(builder->token);
  builder->pos = token_line_position(builder->token);
}


// Peek at our next token without consuming it
static ast_token_id peek_token(build_parser_t* builder)
{
  assert(builder != NULL);

  get_next_token(builder);
  return builder->id;
}


// Get the next token, consuming it
static ast_token_id get_token(build_parser_t* builder)
{
  assert(builder != NULL);

  get_next_token(builder);
  builder->have_token = false;
  return builder->id;
}


// Prevent the token associated with the current token from being freed
static void save_token(build_parser_t* builder)
{
  assert(builder != NULL);
  builder->token = NULL;
}


// Replace the current ID token with an abstract keyword, if it is one
static ast_token_id keyword_replace(build_parser_t* builder)
{
  assert(builder != NULL);

  token_id keyword_id = lexer_is_abstract_keyword(token_string(builder->token));
  if(keyword_id == TK_LEX_ERROR)
    return AT_ID;

  token_set_id(builder->token, keyword_id);
  return AT_TOKEN;
}


/* Load a type description.
 * A type description is the type AST node contained in square brackets.
 * The leading [ must have been loaded before this is called.
 */
static ast_t* get_type(build_parser_t* builder, ast_t* parent)
{
  if(parent == NULL)
  {
    build_error(builder, "Type with no containing node");
    return NULL;
  }

  if(ast_type(parent) != NULL)
  {
    build_error(builder, "Node has multiple types");
    return NULL;
  }

  ast_t* type = get_nodes(builder, AT_RSQUARE);

  if(type != NULL) // Type has to be reversed separately to rest of tree
    ast_reverse(type);

  return type;
}


/* Process a scope attribute.
 * The scope keyword should be found before calling this, but nothing else.
 */
static bool scope_attribute(build_parser_t* builder, ast_t* node)
{
  assert(builder != NULL);

  ast_scope(node);

  while(peek_token(builder) == AT_ID)
  {
    get_token(builder);
    const char* name = stringtab(token_string(builder->token));
    add_subtree_ref(builder, node, name, name);
  }

  return true;
}


/* Process a definition attribute.
* The def keyword should be found before calling this, but nothing else.
*/
static bool def_attribute(build_parser_t* builder, ast_t* node)
{
  assert(builder != NULL);

  if(get_token(builder) != AT_ID)
  {
    build_error(builder, "Expected {def name}");
    return false;
  }

  const char* def_name = stringtab(token_string(builder->token));

  return add_subtree_name(builder, def_name, node);
}


/* Process a dataref attribute.
* The dataref keyword should be found before calling this, but nothing else.
*/
static bool dataref_attribute(build_parser_t* builder, ast_t* node)
{
  assert(builder != NULL);

  if(get_token(builder) != AT_ID)
  {
    build_error(builder, "Expected {dataref name}");
    return false;
  }

  const char* ref_name = stringtab(token_string(builder->token));
  add_subtree_ref(builder, node, ref_name, NULL);
  return true;
}


/* Load node attributes, if any.
 * Attributes are a list of keywords, each prefixed with a colon, following a
 * node name. For example:
 *    seq:scope
 *
 * The node name must have been parsed before calling this, but not the colon.
 */
static ast_t* get_attributes(build_parser_t* builder, ast_t* node)
{
  assert(builder != NULL);

  while(peek_token(builder) == AT_LBRACE)
  {
    get_token(builder);

    if(get_token(builder) != AT_ID)
    {
      build_error(builder, "Expected attribute in {}");
      return NULL;
    }

    const char* attr = token_string(builder->token);

    if(strcmp("scope", attr)==0)
    {
      if(!scope_attribute(builder, node))
        return NULL;
    }
    else if(strcmp("def", attr) == 0)
    {
      if(!def_attribute(builder, node))
        return NULL;
    }
    else if(strcmp("dataref", attr) == 0)
    {
      if(!dataref_attribute(builder, node))
        return NULL;
    }
    else
    {
      build_error(builder, "Unrecognised attribute \"%s\"", attr);
      return NULL;
    }

    if(get_token(builder) != AT_RBRACE)
    {
      build_error(builder, "Expected } after attribute %s", attr);
      return NULL;
    }
  }

  return node;
}


/* Load an ID node.
 * IDs are indicated by the keyword id followed by the ID name, all contained
 * within parentheses. For example:
 *    (id foo)
 *
 * The ( and id keyword must have been parsed before this is called.
 */
static ast_t* get_id(build_parser_t* builder, ast_t* existing_ast)
{
  assert(builder != NULL);

  if(existing_ast != NULL)
  {
    ast_free(existing_ast);
    build_error(builder, "Seen ID not first in node");
    return NULL;
  }

  if(get_token(builder) != AT_ID)
  {
    build_error(builder, "ID name expected");
    return NULL;
  }

  ast_t* ast = ast_token(builder->token);
  ast_setid(ast, TK_ID);
  save_token(builder);

  if(get_token(builder) != AT_RPAREN)
  {
    build_error(builder, "Close paren expected for ID");
    ast_free(ast);
    return NULL;
  }

  return ast;
}


// Load a sequence of nodes until the specified terminator is found
static ast_t* get_nodes(build_parser_t* builder, ast_token_id terminator)
{
  assert(builder != NULL);

  ast_t* ast = NULL;

  while(true)
  {
    ast_token_id id = get_token(builder);
    ast_t* child = NULL;
    bool is_type = false;

    if(id == terminator)
    {
      if(ast == NULL)
        build_error(builder, "Syntax error");

      return ast;
    }

    if(id == AT_ID)
      id = keyword_replace(builder);

    switch(id)
    {
      case AT_LPAREN:
        child = get_nodes(builder, AT_RPAREN);
        break;

      case AT_LSQUARE:
        child = get_type(builder, ast);
        is_type = true;
        break;

      case AT_ERROR:  // Propogate
        break;

      case AT_TOKEN:
        child = ast_token(builder->token);
        save_token(builder);
        get_attributes(builder, child);
        break;

      case AT_ID:
        if(strcmp("id", token_string(builder->token)) == 0)
          return get_id(builder, ast);

        build_error(builder, "Unrecognised identifier \"%s\"",
          token_string(builder->token));
        break;

      default:
        build_error(builder, "Syntax error");
        break;
    }

    if(child == NULL)
    {
      // An error occurred and should already have been reported
      ast_free(ast);
      return NULL;
    }

    if(ast == NULL)
      ast = child;
    else if(is_type)
      ast_settype(ast, child);
    else
      ast_add(ast, child);
  }
}


// Turn the given description into an ast
static ast_t* build_ast(source_t* source, symtab_t* symtab)
{
  assert(source != NULL);
  assert(symtab != NULL);

  // Setup parser
  build_parser_t build_parser;

  build_parser.source = source;
  build_parser.token = NULL;
  build_parser.defs = symtab;
  build_parser.refs = NULL;
  build_parser.line = 1;
  build_parser.pos = 1;
  build_parser.have_token = false;
  build_parser.had_error = false;

  build_parser.lexer = lexer_open(source);
  assert(build_parser.lexer != NULL);

  // Parse given start rule
  ast_t* ast = get_nodes(&build_parser, AT_EOF);

  if(ast != NULL)
  {
    if(!process_refs(&build_parser))
    {
      ast_free(ast);
      ast = NULL;
    }
  }

  ast_reverse(ast);

  // Tidy up parser
  lexer_close(build_parser.lexer);
  builder_ref_t* p = build_parser.refs;

  while(p != NULL)
  {
    builder_ref_t* next = p->next;
    free(p);
    p = next;
  }

  return ast;
}


// Add a source to the builder
static void add_source(builder_t* builder, source_t* source)
{
  assert(builder != NULL);
  assert(source != NULL);

  builder_src_t* newsrc = (builder_src_t*)malloc(sizeof(builder_src_t));
  newsrc->source = source;
  newsrc->next = builder->sources;
  builder->sources = newsrc;
}


builder_t* builder_create(const char* description)
{
  if(description == NULL)
    return NULL;

  source_t* source = source_open_string(description);
  symtab_t* symtab = symtab_create(10);

  ast_t* ast = build_ast(source, symtab);

  if(ast == NULL)
  {
    // Error, tidy up
    source_close(source);
    symtab_free(symtab);
    return NULL;
  }

  // Success, create builder
  builder_t* builder = (builder_t*)malloc(sizeof(builder_t));

  builder->sources = NULL;
  builder->defs = symtab;
  builder->dummy_root = ast_blank(TK_TEST);

  ast_add(builder->dummy_root, ast);
  add_source(builder, source);

  return builder;
}


static ast_t* builder_add_ast(builder_t* builder, const char* description)
{
  assert(builder != NULL);
  assert(description != NULL);

  source_t* source = source_open_string(description);
  ast_t* ast = build_ast(source, builder->defs);

  if(ast == NULL)
  {
    // Error, tidy up
    source_close(source);
    return NULL;
  }

  // Success, add new source to builder
  add_source(builder, source);

  return ast;
}


ast_t* builder_add(builder_t* builder, const char* add_point,
  const char* description)
{
  assert(add_point != NULL);
  assert(description != NULL);

  if(builder == NULL)
    return NULL;

  ast_t* add_parent = builder_find_sub_tree(builder, add_point);

  if(add_parent == NULL)
  {
    error(NULL, 0, 0, "Node with attribute {def %s} not found", add_point);
    return NULL;
  }

  ast_t* ast = builder_add_ast(builder, description);

  if(ast == NULL)
    return NULL;

  // Success, add new tree to builder
  ast_add(add_parent, ast);
  return ast;
}


ast_t* builder_add_type(builder_t* builder, const char* type_of,
  const char* description)
{
  assert(type_of != NULL);
  assert(description != NULL);

  if(builder == NULL)
    return NULL;

  ast_t* type_parent = builder_find_sub_tree(builder, type_of);

  if(type_parent == NULL)
  {
    error(NULL, 0, 0, "Node with attribute {def %s} not found", type_of);
    return NULL;
  }

  assert(ast_type(type_parent) == NULL);
  ast_t* ast = builder_add_ast(builder, description);

  if(ast == NULL)
    return NULL;

  // Success, add new type to builder
  ast_settype(type_parent, ast);
  return ast;
}


ast_t* builder_get_root(builder_t* builder)
{
  if(builder == NULL)
    return NULL;

  assert(builder->dummy_root != NULL);
  return ast_child(builder->dummy_root);
}

/*
ast_t* builder_extract_root(builder_t* builder)
{
  if(builder == NULL)
    return NULL;

  assert(builder->dummy_root != NULL);
  ast_t* root;
  ast_t** children[1] = {&root};
  ast_extract_children(builder->dummy_root, 1, children);
  return root;
}
*/

ast_t* builder_find_sub_tree(builder_t* builder, const char* name)
{
  if(builder == NULL)
    return NULL;

  return (ast_t*)symtab_get(builder->defs, stringtab(name));
}


void builder_free(builder_t* builder)
{
  if(builder == NULL)
    return;

  ast_free(builder->dummy_root);
  symtab_free(builder->defs);

  builder_src_t* p = builder->sources;

  while(p != NULL)
  {
    builder_src_t* next = p->next;
    source_close(p->source);
    free(p);
    p = next;
  }
}


static bool compare_asts(ast_t* prev, ast_t* expected, ast_t* actual,
  bool check_siblings)
{
  assert(prev != NULL);

  if(expected == NULL && actual == NULL)
    return true;

  if(actual == NULL)
  {
    ast_error(expected, "Expected AST %s not found", ast_get_print(expected));
    return false;
  }

  if(expected == NULL)
  {
    ast_error(prev, "Unexpected AST node found, %s", ast_get_print(actual));
    return false;
  }

  token_id expected_id = ast_id(expected);
  token_id actual_id = ast_id(actual);

  // Allow unary and binary minuses to match
  if(expected_id == TK_UNARY_MINUS) expected_id = TK_MINUS;
  if(actual_id == TK_UNARY_MINUS)   actual_id = TK_MINUS;

  if(expected_id != actual_id)
  {
    ast_error(expected, "AST ID mismatch, got %d (%s), expected %d (%s)",
      ast_id(actual), ast_get_print(actual),
      ast_id(expected), ast_get_print(expected));
    return false;
  }

  if(ast_id(expected) == TK_ID && ast_name(actual)[0] == '$' &&
    strcmp(ast_name(expected), "hygid") == 0)
  {
    // Allow expected "hygid" to match any hygenic ID
  }
  else if(strcmp(ast_get_print(expected), ast_get_print(actual)) != 0)
  {
    ast_error(expected, "AST text mismatch, got %s, expected %s",
      ast_get_print(actual), ast_get_print(expected));
    return false;
  }

  if(ast_has_scope(expected) && !ast_has_scope(actual))
  {
    ast_error(expected, "AST missing scope");
    return false;
  }

  if(!ast_has_scope(expected) && ast_has_scope(actual))
  {
    ast_error(actual, "Unexpected AST scope");
    return false;
  }

  if(!compare_asts(expected, ast_child(expected), ast_child(actual), true) ||
    !compare_asts(expected, ast_type(expected), ast_type(actual), true))
    return false;

  return !check_siblings ||
    compare_asts(expected, ast_sibling(expected), ast_sibling(actual), true);
}


bool build_compare_asts(ast_t* expected, ast_t* actual)
{
  assert(expected != NULL);
  assert(actual != NULL);

  return compare_asts(expected, expected, actual, true);
}


bool build_compare_asts_no_sibling(ast_t* expected, ast_t* actual)
{
  assert(expected != NULL);
  assert(actual != NULL);

  return compare_asts(expected, expected, actual, false);
}
