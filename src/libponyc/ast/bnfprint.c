#include <platform.h>
#include "bnfprint.h"
#include "lexer.h"
#include "ast.h"
#include "token.h"
#include "../../libponyrt/mem/pool.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/** This file contains the BNF printer, which prints out the Pony BNF in human
 * readable and ANTLR file form. This is intended as a form of documentation,
 * as well as allowing us to use ANTLR to check for grammar errors (such as
 * ambiguities). Since the printed gramamr is generated from the actual parse
 * macros we are guaranteed that it is accurate and up to date.
 *
 * We generate a BNF tree structure from the parser source. To do this we
 * define an alternative set of the parse macros defined in parserapi.h, and
 * then #include parser.c within this file.
 *
 * Having done that we tidy up the BNF somewhat. This is mainly required
 * because the macros include extra information for building the AST, which we
 * do not require.
 *
 * We also strip out all the lexer test tokens, since they are not part of the
 * Pony grammar as such. However, we do provide the option of a "raw" grammar
 * that includes these so we can verify they do not break the grammar.
 *
 * The resulting grammar is then simply printed to stdout with appropriate
 * formatting.
 */


// Fixed text to add to printed BNF to make complete antlr file
static const char* antlr_pre =
  "// ANTLR v3 grammar\n"
  "grammar pony;\n\n"
  "options\n"
  "{\n"
  "  output = AST;\n"
  "  k = 1;\n"
  "}\n\n"
  "// Parser\n";

static const char* antlr_post =
  "// Rules of the form antlr_* are only present to avoid a bug in the\n"
  "// interpreter\n\n"
  "/* Precedence\n\n"
  "Value:\n"
  "1. postfix\n"
  "2. unop\n"
  "3. binop\n"
  "4. =\n"
  "5. seq\n"
  "6. ,\n\n"
  "Type:\n"
  "1. ->\n"
  "2. & |\n"
  "3. ,\n"
  "*/\n\n"
  "// Lexer\n\n"
  "ID\n"
  "  : LETTER (LETTER | DIGIT | '_' | '\\'')*\n"
  "  | '_' (LETTER | DIGIT | '_' | '\\'')+\n"
  "  ;\n\n"
  "INT\n"
  "  : DIGIT (DIGIT | '_')*\n"
  "  | '0' 'x' (HEX | '_')+\n"
  "  | '0' 'b' (BINARY | '_')+\n"
  "  | '\\'' CHAR_CHAR* '\\''\n"
  "  ;\n\n"
  "FLOAT\n"
  "  : DIGIT (DIGIT | '_')* ('.' DIGIT (DIGIT | '_')*)? EXP?\n"
  "  ;\n\n"
  "STRING\n"
  "  : '\"' STRING_CHAR* '\"'\n"
  "  | '\"\"\"' (('\"' | '\"\"') ? ~'\"')* '\"\"\"' '\"'*\n"
  "  ;\n\n"
  "LPAREN_NEW\n"
  "  : NEWLINE '('\n"
  "  ;\n\n"
  "LSQUARE_NEW\n"
  "  : NEWLINE '['\n"
  "  ;\n\n"
  "MINUS_NEW\n"
  "  : NEWLINE '-'\n"
  "  ;\n\n"
  "LINECOMMENT\n"
  "  : '//' ~('\\n')* {$channel = HIDDEN;}\n"
  "  ;\n\n"
  "NESTEDCOMMENT\n"
  "  : '/*' (NESTEDCOMMENT | '/' ~'*' | ~('*' | '/') | "
  "('*'+ ~('*' | '/')))* '*'+ '/' {$channel = HIDDEN;}\n"
  "  ;\n\n"
  "WS\n"
  "  : (' ' | '\\t' | '\\r')+ {$channel = HIDDEN;}\n"
  "  ;\n\n"
  "NEWLINE\n"
  "  : '\\n' (' ' | '\\t' | '\\r')* {$channel = HIDDEN;}\n"
  "  ;\n\n"
  "fragment\n"
  "CHAR_CHAR\n"
  "  : ESC\n"
  "  | ~('\\'' | '\\\\')\n"
  "  ;\n\n"
  "fragment\n"
  "STRING_CHAR\n"
  "  : ESC\n"
  "  | ~('\"' | '\\\\')\n"
  "  ;\n\n"
  "fragment\n"
  "EXP\n"
  "  : ('e' | 'E') ('+' | '-')? (DIGIT | '_')+\n"
  "  ;\n\n"
  "fragment\n"
  "LETTER\n"
  "  : 'a'..'z' | 'A'..'Z'\n"
  "  ;\n\n"
  "fragment\n"
  "BINARY\n"
  "  : '0'..'1'\n"
  "  ;\n\n"
  "fragment\n"
  "DIGIT\n"
  "  : '0'..'9'\n"
  "  ;\n\n"
  "fragment\n"
  "HEX\n"
  "  : DIGIT | 'a'..'f' | 'A'..'F'\n"
  "  ;\n\n"
  "fragment\n"
  "ESC\n"
  "  : '\\\\' ('a' | 'b' | 'e' | 'f' | 'n' | 'r' | 't' | 'v' | '\\\"' | "
  "'\\\\' | '0')\n"
  "  | HEX_ESC\n"
  "  | UNICODE_ESC\n"
  "  | UNICODE2_ESC\n"
  "  ;\n\n"
  "fragment\n"
  "HEX_ESC\n"
  "  : '\\\\' 'x' HEX HEX\n"
  "  ;\n\n"
  "fragment\n"
  "UNICODE_ESC\n"
  "  : '\\\\' 'u' HEX HEX HEX HEX\n"
  "  ;\n\n"
  "fragment\n"
  "UNICODE2_ESC\n"
  "  : '\\\\' 'U' HEX HEX HEX HEX HEX HEX\n"
  "  ;\n";


typedef enum bnf_id
{
  BNF_TREE,   // Top level node for the whole grammar
  BNF_DEF,    // Rule definition, has name
  BNF_SEQ,    // A sequence, ie A followed by B followed by C
  BNF_OR,     // An or, ie A or B or C
  BNF_REPEAT, // A repetition, ie A*, any number of As
  BNF_TOKEN,  // A reference to a lexical token
  BNF_QUOTED_TOKEN,  // A reference to a lexical token printed in quotes
  BNF_RULE,   // A reference to a rule
  BNF_NEVER,  // Never matches, only used internally
  BNF_NOP     // Node does nothing, only used internally
} bnf_id;


typedef struct bnf_t
{
  bnf_id id;
  const char* name;
  int hack_count;
  bool optional;
  bool used;
  bool inline_rule;

  // Each node has some number of children arranged in a list, with pointers to
  // the first and last
  struct bnf_t* child;
  struct bnf_t* last_child;
  struct bnf_t* sibling;
} bnf_t;


// Forward declarations
static void bnf_print_children(bnf_t* bnf, const char* separator,
  bool parens_ok, bool children_rule_top);

static void bnf_simplify_children(bnf_t* tree, bnf_t* list, bool* out_never,
  bool* out_nop, bool *out_changed);


static bnf_t* bnf_create(bnf_id id)
{
  bnf_t* b = POOL_ALLOC(bnf_t);
  memset(b, 0, sizeof(bnf_t));
  b->id = id;

  return b;
}


static void bnf_free(bnf_t* bnf)
{
  if(bnf == NULL)
    return;

  // Names aren't freed, they should all be literals
  bnf_free(bnf->child);
  bnf_free(bnf->sibling);
  POOL_FREE(bnf_t, bnf);
}


// Copy a sub tree
static bnf_t* bnf_copy(bnf_t* bnf, bnf_t** out_last_sibling)
{
  if(bnf == NULL)
    return NULL;

  bnf_t* new_bnf = bnf_create(bnf->id);
  new_bnf->name = bnf->name;
  new_bnf->optional = bnf->optional;
  new_bnf->used = bnf->used;
  new_bnf->inline_rule = bnf->inline_rule;

  if(out_last_sibling != NULL)
    *out_last_sibling = new_bnf;

  new_bnf->sibling = bnf_copy(bnf->sibling, out_last_sibling);
  new_bnf->child = bnf_copy(bnf->child, &new_bnf->last_child);

  return new_bnf;
}


// Add the given new BNF node to the given parent.
// New children go at the end of the child list.
static bnf_t* bnf_add(bnf_t* bnf, bnf_t* parent)
{
  assert(bnf != NULL);
  assert(parent != NULL);

  if(parent->last_child == NULL)  // First node in list
    parent->child = bnf;
  else
    parent->last_child->sibling = bnf;

  parent->last_child = bnf;
  return bnf;
}


// Print out the given node, in ANTLR syntax.
// The top_format parameter indicates we should use top level node within rule
// formatting, which is slightly different to normal.
static void bnf_print(bnf_t* bnf, bool top_format)
{
  if(bnf == NULL)
    return;

  switch(bnf->id)
  {
    case BNF_TREE:
      bnf_print_children(bnf, "", true, true);
      break;

    case BNF_DEF:
      // Only print marked rule and hack rules
      if(bnf->used || bnf->name == NULL)
      {
        if(bnf->name == NULL)
          printf("antlr_%d\n  : ", bnf->hack_count);
        else
          printf("%s\n  : ", bnf->name);

        bnf_print(bnf->child, true);
        printf("\n  ;\n\n");
      }
      break;

    case BNF_SEQ:
      bnf_print_children(bnf, " ", top_format, false);
      break;

    case BNF_OR:
      if(top_format && !bnf->optional)
        bnf_print_children(bnf, "\n  | ", true, true);
      else
        bnf_print_children(bnf, " | ", false, false);

      if(bnf->optional)
        printf("?");

      break;

    case BNF_REPEAT:
      bnf_print(bnf->child, false);
      printf("*");
      break;

    case BNF_TOKEN:
    case BNF_RULE:
      if(bnf->name == NULL)
        printf("antlr_%d", bnf->hack_count);
      else
        printf("%s", bnf->name);
      break;

    case BNF_QUOTED_TOKEN:
      printf("'%s'", bnf->name);
      break;

    case BNF_NEVER:
      printf("NEVER");
      break;

    case BNF_NOP:
      printf("nop");
      break;

    default:
      assert(false);
      break;
  }
}


// Print out the children of the given node, in ANTLR syntax.
// The top_format parameters indicate we should use top level node within rule
// formatting, which is slightly different to normal.
static void bnf_print_children(bnf_t* bnf, const char* separator,
  bool top_format, bool children_top_format)
{
  assert(bnf != NULL);
  assert(separator != NULL);

  bnf_t* child = bnf->child;
  assert(child != NULL);

  bool parens = !top_format && (child->sibling != NULL);

  if(parens)
    printf("(");

  bnf_print(child, children_top_format);

  for(bnf_t* p = child->sibling; p != NULL; p = p->sibling)
  {
    printf("%s", separator);
    bnf_print(p, children_top_format);
  }

  if(parens)
    printf(")");
}


// Build a list of token references with the given node
static void bnf_token_set(bnf_t* bnf, token_id* tokens, bool clean)
{
  assert(bnf != NULL);

  for(int i = 0; tokens[i] != TK_NONE; i++)
  {
    bnf_t* p = bnf_add(bnf_create(BNF_TOKEN), bnf);
    assert(p != NULL);

    //token_id next = tokens[i + 1];

    switch(tokens[i])
    {
      // Special case tokens
      case TK_EOF: p->name = ""; break;
      case TK_STRING: p->name = "STRING"; break;
      case TK_INT: p->name = "INT"; break;
      case TK_FLOAT: p->name = "FLOAT"; break;
      case TK_ID: p->name = "ID"; break;
      case TK_LPAREN_NEW: p->name = "LPAREN_NEW"; break;
      case TK_LSQUARE_NEW: p->name = "LSQUARE_NEW"; break;
      case TK_MINUS_NEW: p->name = "MINUS_NEW"; break;

      default:
        // Fixed text tokens: keywords, symbols, etc
        p->name = lexer_print(tokens[i]);
        p->id = BNF_QUOTED_TOKEN;

        assert(p->name != NULL);

        if((clean && p->name[0] == '$') || tokens[i] == TK_NEWLINE)
        {
          // Remove unclean symbol
          p->id = BNF_NEVER;
        }

        break;
    }
  }
}


// Build a list of rule references with the given node
static void bnf_rule_set(bnf_t* bnf, const char** rules)
{
  assert(bnf != NULL);

  for(int i = 0; rules[i] != NULL; i++)
  {
    bnf_t* p = bnf_add(bnf_create(BNF_RULE), bnf);
    assert(p != NULL);
    p->name = rules[i];
  }
}


// Use the given node's child instead of the node.
// We don't actually remove the given node, we just copy the child's state over
// it and then free the child.
static void bnf_use_child(bnf_t* bnf)
{
  assert(bnf != NULL);
  assert(bnf->child != NULL);

  bnf_t* child = bnf->child;

  bnf->id = child->id;
  bnf->name = child->name;
  bnf->optional = child->optional;
  bnf->child = child->child;
  bnf->last_child = child->last_child;

  child->child = NULL;
  bnf_free(child);
}


// Find the rule with the specified name
static bnf_t* bnf_find_def(bnf_t* tree, const char* name)
{
  assert(tree != NULL);
  assert(name != NULL);

  for(bnf_t* p = tree->child; p != NULL; p = p->sibling)
  {
    if(strcmp(p->name, name) == 0)  // Match found
      return p;
  }

  // Not found, this is impossible if the parser compiles
  assert(false);
  return NULL;
}


// Attempt to simplify the given node.
// We simplify from the bottom up, removing subrules that can never match or do
// nothing. We also inline trivial rules when they are referenced.
// The out_changed variable is set to true when any simplifications occur.
static void bnf_simplify_node(bnf_t* tree, bnf_t* bnf, bool *out_changed)
{
  assert(bnf != NULL);
  assert(out_changed != NULL);

  switch(bnf->id)
  {
    case BNF_TREE:
      bnf_simplify_children(tree, bnf, NULL, NULL, out_changed);
      break;

    case BNF_DEF:
      bnf_simplify_node(tree, bnf->child, out_changed);
      break;

    case BNF_SEQ:
    {
      bool any_never = false;
      bnf_simplify_children(tree, bnf, &any_never, NULL, out_changed);

      if(any_never)
      {
        bnf->id = BNF_NEVER;
        *out_changed = true;
      }
      else if(bnf->child == NULL)
      {
        // Empty sequence
        bnf->id = BNF_NOP;
        *out_changed = true;
      }
      else if(bnf->child->sibling == NULL)
      {
        // Lone node in sequence
        bnf_use_child(bnf);
        *out_changed = true;
      }

      break;
    }

    case BNF_OR:
    {
      bool any_nop = false;
      bnf_simplify_children(tree, bnf, NULL, &any_nop, out_changed);

      if(any_nop)
      {
        bnf->optional = true;
        *out_changed = true;
      }

      if(bnf->child == NULL)
      {
        // Empty set
        bnf->id = (bnf->optional) ? BNF_NOP : BNF_NEVER;
        *out_changed = true;
      }
      else if(bnf->child->sibling == NULL && !bnf->optional)
      {
        // Lone node in or
        bnf_use_child(bnf);
        *out_changed = true;
      }

      break;
    }

    case BNF_REPEAT:
      bnf_simplify_children(tree, bnf, NULL, NULL, out_changed);

      if(bnf->child == NULL)
      {
        // Empty body
        bnf->id = BNF_NOP;
        *out_changed = true;
      }

      break;

    case BNF_RULE:
    {
      // Check for inlinable rules
      if(bnf->name == NULL) // Hack rules aren't inlinable
        break;

      bnf_t* def = bnf_find_def(tree, bnf->name);
      assert(def != NULL);

      bnf_t* rule = def->child;
      assert(rule != NULL);

      // We inline rules that are nevers, nops, single token / rule references
      // or have been explicitly marked to inline
      if(rule->id == BNF_NEVER || rule->id == BNF_NOP ||
        rule->id == BNF_TOKEN || rule->id == BNF_QUOTED_TOKEN ||
        rule->id == BNF_RULE || def->inline_rule)
      {
        // Inline rule
        bnf->id = rule->id;
        bnf->name = rule->name;
        bnf->optional = rule->optional;
        bnf->last_child = NULL;
        bnf->child = bnf_copy(rule->child, &bnf->last_child);

        // Child of def should only ever have one child, so don't need to worry
        // about copying siblings
        assert(rule->sibling == NULL);

        // Don't worry about simplifying children now, leave that til the next
        // iteration
        *out_changed = true;
      }

      break;
    }

    default:
      break;
  }
}


// Simplify the children of the given node, removing any that never match or do
// nothing.
// The out_changed variable is set to true when any simplifications occur.
static void bnf_simplify_children(bnf_t* tree, bnf_t* parent, bool* out_never,
  bool* out_nop, bool *out_changed)
{
  assert(parent != NULL);

  if(out_never != NULL) *out_never = false;
  if(out_nop != NULL) *out_nop = false;

  // Run through the child list
  bnf_t* prev = NULL;
  bnf_t* p = parent->child;

  while(p != NULL)
  {
    // Simplify the child
    bnf_simplify_node(tree, p, out_changed);

    if(p->id == BNF_NEVER && out_never != NULL)
      *out_never = true;

    if(p->id == BNF_NOP && out_nop != NULL)
      *out_nop = true;

    if(p->id == BNF_NEVER || p->id == BNF_NOP)
    {
      // Remove this child
      if(prev == NULL)  // Removing first node in list
        parent->child = p->sibling;
      else
        prev->sibling = p->sibling;

      bnf_t* next = p->sibling;
      p->sibling = NULL;
      bnf_free(p);
      p = next;
      *out_changed = true;
    }
    else
    {
      prev = p;
      p = p->sibling;
    }
  }

  parent->last_child = prev;
}


// Simplify the given tree as far as possible
static void bnf_simplify(bnf_t* tree)
{
  assert(tree != NULL);

  bool changed = true;

  while(changed)
  {
    changed = false;
    bnf_simplify_node(tree, tree, &changed);
  }
}


// Add extra rules to get round the ANTLR interpreter bug
static void bnf_avoid_antlr_bug(bnf_t* tree, bnf_t* bnf)
{
  assert(tree != NULL);

  if(bnf == NULL)
    return;

  // First recurse into children
  for(bnf_t* p = bnf->child; p != NULL; p = p->sibling)
    bnf_avoid_antlr_bug(tree, p);

  // We only care about cases where the 2nd child of an 'or' node immediately
  // inside a 'repeat' node is a rule or sub-rule
  if(bnf->id != BNF_REPEAT)
    return;

  bnf_t* or_node = bnf->child;
  assert(or_node != NULL);

  if(or_node->id != BNF_OR)
    return;

  assert(or_node->child != NULL);

  bnf_t* second_child = or_node->child->sibling;

  if(second_child == NULL || second_child->id == BNF_TOKEN ||
    second_child->id == BNF_QUOTED_TOKEN)
    return;

  // This is the bug case. Move 'or' node into its own rule.
  int rule_no = tree->hack_count++;

  assert(tree->last_child != NULL);
  bnf_t* new_rule = bnf_create(BNF_DEF);
  new_rule->hack_count = rule_no;
  new_rule->child = or_node;

  tree->last_child->sibling = new_rule;
  tree->last_child = new_rule;

  bnf_t* new_ref = bnf_create(BNF_RULE);
  new_ref->hack_count = rule_no;
  bnf->child = new_ref;
  bnf->last_child = new_ref;
}


// Mark rule definitions that are referenced from within the given subtree
static void bnf_mark_refd_defs(bnf_t* tree, bnf_t* bnf)
{
  assert(tree != NULL);
  assert(bnf != NULL);

  for(bnf_t* p = bnf; p != NULL; p = p->sibling)
  {
    if(p->child != NULL)
      bnf_mark_refd_defs(tree, p->child);

    if(p->id == BNF_RULE && p->name != NULL)
    {
      bnf_t* rule = bnf_find_def(tree, p->name);
      assert(rule != NULL);
      rule->used = true;
    }
  }
}


// Mark rule definitions that are used
static void bnf_mark_used_rules(bnf_t* tree)
{
  // The first rule defined has an implicit reference, the entry point
  tree->child->used = true;

  bnf_mark_refd_defs(tree, tree->child);
}


// Macro to help convert __VAR_ARGS__ into a string list
#define STRINGIFY(x) #x,

// Parse macros we don't need
#define DECL(rule)
#define RESTART(...)
#define AST_NODE(ID)
#define MAP_ID(FROM, TO)
#define SCOPE()
#define CUSTOMBUILD(builder_fn)
#define INFIX_BUILD()
#define INFIX_REVERSE()
#define REORDER(...)
#define WRAP(child_idx, id)
#define REWRITE(body)
#define SET_FLAG(f)
#define NEXT_FLAGS(f)

#define DEF(rule_name) \
  { \
    bnf_t* rule = bnf_create(BNF_DEF); \
    rule->name = #rule_name; \
    rule->sibling = parent->child; \
    parent->child = rule; \
    bnf_t* parent = bnf_add(bnf_create(BNF_SEQ), rule);

#define OPT optional = true;
#define OPT_DFLT(id) optional = true;
#define OPT_NO_DFLT optional = true;

#define TOKEN(desc, ...) \
  { \
    static token_id tokens[] = { __VA_ARGS__, TK_NONE }; \
    bnf_t* p = bnf_add(bnf_create(BNF_OR), parent); \
    p->optional = optional; \
    optional = false; \
    bnf_token_set(p, tokens, clean); \
  }

#define SKIP(desc, ...) TOKEN(desc, __VA_ARGS__)

#define RULE(desc, ...) \
  { \
    static const char* rules[] = { FOREACH(STRINGIFY, __VA_ARGS__) NULL }; \
    bnf_t* p = bnf_add(bnf_create(BNF_OR), parent); \
    p->optional = optional; \
    optional = false; \
    bnf_rule_set(p, rules); \
  }

#define IF(id, body) \
  { \
    bnf_t* p = bnf_add(bnf_create(BNF_OR), parent); \
    p->optional = true; \
    bnf_t* parent = bnf_add(bnf_create(BNF_SEQ), p); \
    TOKEN(desc, id); \
    body; \
  }

#define IFELSE(id, thenbody, elsebody) \
  { \
    bnf_t* p = bnf_add(bnf_create(BNF_OR), parent); \
    bnf_t* parent = bnf_add(bnf_create(BNF_SEQ), p); \
    TOKEN(desc, id); \
    thenbody; \
    parent = bnf_add(bnf_create(BNF_SEQ), p); \
    elsebody; \
  }

#define WHILE(id, body)  \
  { \
    bnf_t* p = bnf_add(bnf_create(BNF_REPEAT), parent); \
    bnf_t* parent = bnf_add(bnf_create(BNF_SEQ), p); \
    TOKEN(desc, id); \
    body; \
  }

#define SEQ(desc, ...)  \
  { \
    static const char* tokens[] = { FOREACH(STRINGIFY, __VA_ARGS__) NULL }; \
    bnf_t* p = bnf_add(bnf_create(BNF_REPEAT), parent); \
    p = bnf_add(bnf_create(BNF_OR), p); \
    bnf_rule_set(p, tokens); \
  }

#define DONE()  }


// Macros specific to us that do nothing for parsing

// Inline this rule when printing
#define PRINT_INLINE() rule->inline_rule = true


// Prevent other version of macros being included
#define PARSERAPI_H
#define PARSER_H


// Get our grammar definition based on the parse macros
static bnf_t* bnf_def(bool clean)
{
  bnf_t* parent = bnf_create(BNF_TREE);
  bool optional = false;

#include "parser.c"

  return parent;
}


// Main function to print out grammar
void print_grammar(bool antlr, bool clean)
{
  bnf_t* tree = bnf_def(clean);
  assert(tree != NULL);

  bnf_simplify(tree);

  if(antlr)
    bnf_avoid_antlr_bug(tree, tree);

  bnf_mark_used_rules(tree);  // We only print rules that are used

  if(antlr)
    printf("%s\n", antlr_pre);

  bnf_print(tree, false);

  if(antlr)
    printf("%s\n", antlr_post);
}
