#include "docgen.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


// Define a type with the docgen state that needs to passed around the
// functions below.
// The doc directory is the root directory that we put our generated files
// into. This is supplied by the caller, presumably from a command line arg.
// Most of the time we have 2 files open:
// 1. The index file. This is the mkdocs yaml file that specifies the structure
// of the document and lives in the doc directory.
// This file is opened in the top level function before any processing occurs
// and is closed by the same function once we've processed everything.
// 2. The type file. This is the md file for a specific type and lives in the
// "docs" sub directory of the doc directory. We have one file per type.
// This file is opened and closed by the functions that handles the type. When
// closed the file pointer should always be put back to NULL.
typedef struct docgen_t
{
  FILE* index_file;
  FILE* type_file;
  const char* doc_dir;
  size_t doc_dir_len;
} docgen_t;


// Define a list for keeping lists of ASTs ordered by name.
// Each list should have an empty head node (prefereably on the stack). Other
// nodes will be pool alloced, 1 node per AST item.
// Lists are kept ordered by interesting new items in the correct place, with a
// simple linear search.
typedef struct ast_list_t
{
  ast_t* ast;
  const char* name;
  struct ast_list_t* next;
} ast_list_t;


// Free the contents of the given list, but not the head node which must be
// handled separately
static void doc_list_free(ast_list_t* list)
{
  assert(list != NULL);

  ast_list_t* p = list->next;

  while(p != NULL)
  {
    ast_list_t* next = p->next;
    POOL_FREE(ast_list_t, p);
    p = next;
  }
}


// Add the given AST to the given list, under the specified name
static void doc_list_add(ast_list_t* list, ast_t* ast, const char* name)
{
  assert(list != NULL);
  assert(ast != NULL);

  ast_list_t* n = POOL_ALLOC(ast_list_t);
  n->ast = ast;
  n->name = name;
  n->next = NULL;

  // Find where to add name in sorted list
  ast_list_t* prev = list;

  for(ast_list_t* p = prev->next; p != NULL; prev = p, p = p->next)
  {
    assert(p->name != NULL);

    if(strcmp(p->name, name) > 0)
    {
      // Add new node before p
      n->next = p;
      prev->next = n;
      return;
    }
  }

  // Add new node at end of list
  prev->next = n;
}


// Add the given AST to the given list, using the name from the specified
// child.
// ASTs with hygenic names are ignored.
// Leading underscores on names are ignored for sorting purposes.
// When false the allow_public parameter causes ASTs with public names to be
// ignored. allow_private does the same for private names.
static void doc_list_add_named(ast_list_t* list, ast_t* ast, size_t id_index,
  bool allow_public, bool allow_private)
{
  assert(list != NULL);
  assert(ast != NULL);

  const char* name = ast_name(ast_childidx(ast, id_index));
  assert(name != NULL);

  if(name[0] == '$')  // Ignore internally generated names
    return;

  if(name[0] == '_' && !allow_private)  // Ignore private
    return;

  if(name[0] != '_' && !allow_public)  // Ignore public
    return;

  if(name[0] == '_')  // Ignore leading underscore for ordering
    name++;

  doc_list_add(list, ast, name);
}


// Functions to handle full qualified type names (TQFNs)

// We need unique names for types, for use in file names and links. The format
// we use is:
//      qualfied_package_name-type_name
// All /s in the package name are replaced with dashes.
// This may fail if there are 2 or more packages with the same qualified name.
// This is unlikely, but possible and needs to be fixed later.


// Report the length of the TQFN for the given type (excluding terminator)
static size_t tqfn_length(ast_t* type)
{
  assert(type != NULL);

  ast_t* package = ast_nearest(type, TK_PACKAGE);
  assert(package != NULL);

  // We need the qualified package name and the type name
  const char* pkg_qual_name = package_qualified_name(package);
  const char* type_name = ast_name(ast_child(type));

  assert(pkg_qual_name != NULL);
  assert(type_name != NULL);

  size_t pkg_name_len = strlen(pkg_qual_name);
  size_t type_name_len = strlen(type_name);
  size_t len = pkg_name_len + 1 + type_name_len;  // +1 for extra dash

  return len;
}


// Write the TQFN for the given type to the given buffer.
// Note that a terminator will be written and the given buffer MUST be big
// enough to accomodate this.
static void write_tqfn(ast_t* type, char* buffer)
{
  assert(type != NULL);
  assert(buffer != NULL);

  ast_t* package = ast_nearest(type, TK_PACKAGE);
  assert(package != NULL);

  // We need the qualified package name and the type name
  const char* pkg_qual_name = package_qualified_name(package);
  const char* type_name = ast_name(ast_child(type));

  assert(pkg_qual_name != NULL);
  assert(type_name != NULL);

  // Package qualified name
  for(const char* p = pkg_qual_name; *p != '\0'; p++)
  {
    if(*p == '/')
      *(buffer++) = '-';
    else
      *(buffer++) = *p;
  }

  // Dash
  *(buffer++) = '-';

  // Type name
  for(const char* p = type_name; *p != '\0'; p++)
    *(buffer++) = *p;

  *buffer = '\0';
}


// Functions to handle types

static void doc_type_list(docgen_t* docgen, ast_t* list, const char* preamble,
  const char* separator, const char* postamble);


// Write the given type to the current type file
static void doc_type(docgen_t* docgen, ast_t* type)
{
  assert(docgen != NULL);
  assert(docgen->type_file != NULL);
  assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, package, id, tparams, cap, ephemeral);

      // Find type we reference so we can link to it
      ast_t* target = (ast_t*)ast_data(type);
      assert(target != NULL);

      size_t link_len = tqfn_length(target) + 1;  // +1 for terminator
      char* buf = (char*)pool_alloc_size(link_len);
      write_tqfn(target, buf);

      // Links are of the form: [text](target)
      fprintf(docgen->type_file, "[%s](%s)", ast_name(id), buf);
      pool_free_size(link_len, buf);

      doc_type_list(docgen, tparams, "\\[", ", ", "\\]");

      if(ast_id(cap) != TK_NONE)
        fprintf(docgen->type_file, " %s", ast_get_print(cap));

      if(ast_id(ephemeral) != TK_NONE)
        fprintf(docgen->type_file, "%s", ast_get_print(ephemeral));

      break;
    }

    case TK_UNIONTYPE:
      doc_type_list(docgen, type, "(", " | ", ")");
      break;

    case TK_ISECTTYPE:
      doc_type_list(docgen, type, "(", " & ", ")");
      break;

    case TK_TUPLETYPE:
      doc_type_list(docgen, type, "(", " , ", ")");
      break;

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(type, id, cap, ephemeral);
      fprintf(docgen->type_file, "%s", ast_name(id));

      if(ast_id(cap) != TK_NONE)
        fprintf(docgen->type_file, " %s", ast_get_print(cap));

      if(ast_id(ephemeral) != TK_NONE)
        fprintf(docgen->type_file, "%s", ast_get_print(ephemeral));

      break;
    }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);
      doc_type(docgen, left);
      fprintf(docgen->type_file, "->");
      doc_type(docgen, right);
      break;
    }

    case TK_THISTYPE:
      fprintf(docgen->type_file, "this");
      break;

    case TK_BOXTYPE:
      fprintf(docgen->type_file, "box");
      break;

    default:
      assert(0);
  }
}


// Write the given list of types to the current type file, with the specified
// preamble, separator and psotamble text. If the list is empty nothing is
// written.
static void doc_type_list(docgen_t* docgen, ast_t* list, const char* preamble,
  const char* separator, const char* postamble)
{
  assert(docgen != NULL);
  assert(docgen->type_file != NULL);
  assert(list != NULL);
  assert(preamble != NULL);
  assert(separator != NULL);
  assert(postamble != NULL);

  if(ast_id(list) == TK_NONE)
    return;

  fprintf(docgen->type_file, "%s", preamble);

  for(ast_t* p = ast_child(list); p != NULL; p = ast_sibling(p))
  {
    doc_type(docgen, p);

    if(ast_sibling(p) != NULL)
      fprintf(docgen->type_file, "%s", separator);
  }

  fprintf(docgen->type_file, "%s", postamble);
}


// Functions to handle everything else

// Write the given list of fields to the current type file.
// The given title text is used as a section header.
// If the field list is empty nothing is written.
static void doc_fields(docgen_t* docgen, ast_list_t* fields, const char* title)
{
  assert(docgen != NULL);
  assert(docgen->type_file != NULL);
  assert(fields != NULL);
  assert(title != NULL);

  if(fields->next == NULL)  // No fields
    return;

  fprintf(docgen->type_file, "# %s\n\n", title);

  for(ast_list_t* p = fields->next; p != NULL; p = p->next)
  {
    ast_t* field = p->ast;
    assert(field != NULL);

    AST_GET_CHILDREN(field, id, type, init);
    const char* name = ast_name(id);
    assert(name != NULL);

    // Don't want ast_get_print() as that will give us flet or fvar
    fprintf(docgen->type_file, "* %s %s: ",
      (ast_id(field) == TK_VAR) ? "var" : "let", name);

    doc_type(docgen, type);
    fprintf(docgen->type_file, "\n");
  }
}


// Write the given list of type parameters to the current type file, with
// surrounding []. If the given list is empty nothing is written.
static void doc_type_params(docgen_t* docgen, ast_t* t_params)
{
  assert(docgen != NULL);
  assert(docgen->type_file != NULL);
  assert(t_params != NULL);

  if(ast_id(t_params) == TK_NONE)
    return;

  assert(ast_id(t_params) == TK_TYPEPARAMS);

  fprintf(docgen->type_file, "\\[");
  ast_t* first = ast_child(t_params);

  for(ast_t* t_param = first; t_param != NULL; t_param = ast_sibling(t_param))
  {
    if(t_param != first)
      fprintf(docgen->type_file, ", ");

    AST_GET_CHILDREN(t_param, id, constraint, default_type);
    const char* name = ast_name(id);
    assert(name != NULL);

    if(ast_id(default_type) != TK_NONE)
      fprintf(docgen->type_file, "optional ");

    fprintf(docgen->type_file, "%s: ", name);

    if(ast_id(constraint) != TK_NONE)
      doc_type(docgen, constraint);
    else
      fprintf(docgen->type_file, "no constraint");
  }

  fprintf(docgen->type_file, "\\]");
}


// Write the given list of parameters to the current type file, with
// surrounding (). If the given list is empty () is still written.
static void doc_params(docgen_t* docgen, ast_t* params)
{
  assert(docgen != NULL);
  assert(docgen->type_file != NULL);
  assert(params != NULL);

  fprintf(docgen->type_file, "(");
  ast_t* first = ast_child(params);

  for(ast_t* param = first; param != NULL; param = ast_sibling(param))
  {
    if(param != first)
      fprintf(docgen->type_file, ", ");

    AST_GET_CHILDREN(param, id, type, def_val);
    const char* name = ast_name(id);
    assert(name != NULL);

    if(ast_id(def_val) != TK_NONE)
      fprintf(docgen->type_file, "optional ");

    fprintf(docgen->type_file, "%s: ", name);
    doc_type(docgen, type);
  }

  fprintf(docgen->type_file, ")");
}


// Write a description of the given method to the current type file
static void doc_method(docgen_t* docgen, ast_t* method)
{
  assert(docgen != NULL);
  assert(docgen->type_file != NULL);
  assert(method != NULL);

  AST_GET_CHILDREN(method, cap, id, t_params, params, ret, error, body, doc);

  const char* name = ast_name(id);
  assert(name != NULL);

  // Sub heading
  fprintf(docgen->type_file, "## %s %s()\n", ast_get_print(method), name);

  // Reconstruct signature
  fprintf(docgen->type_file, "%s", ast_get_print(method));

  if(ast_id(method) == TK_FUN)
    fprintf(docgen->type_file, " %s\n", ast_get_print(cap));

  fprintf(docgen->type_file, " %s", name);
  doc_type_params(docgen, t_params);
  doc_params(docgen, params);

  if(ast_id(method) == TK_FUN)
  {
    fprintf(docgen->type_file, ": ");
    doc_type(docgen, ret);
  }

  if(ast_id(error) == TK_QUESTION)
    fprintf(docgen->type_file, " ?");

  // Further information
  fprintf(docgen->type_file, "\n\n");
  fprintf(docgen->type_file, "%s", (name[0] == '_') ? "Private" : "Public");

  if(ast_id(error) == TK_QUESTION)
    fprintf(docgen->type_file, ", may raise an error");

  fprintf(docgen->type_file, ".\n\n");

  // Finally the docstring, if any
  if(ast_id(doc) != TK_NONE)
    fprintf(docgen->type_file, "%s\n\n", ast_name(doc));
}


// Write the given list of methods to the current type file.
// The variety text is used as a heading.
// If the list is empty nothing is written.
static void doc_methods(docgen_t* docgen, ast_list_t* methods,
  const char* variety)
{
  assert(docgen != NULL);
  assert(docgen->type_file != NULL);
  assert(methods != NULL);
  assert(variety != NULL);

  if(methods->next == NULL)
    return;

  fprintf(docgen->type_file, "# %s\n\n", variety);

  for(ast_list_t* p = methods->next; p != NULL; p = p->next)
    doc_method(docgen, p->ast);
}


// Write a description of the given entity to the current type file.
// The containing package is handed in to save looking it up again.
static void doc_entity(docgen_t* docgen, ast_t* ast, ast_t* package)
{
  assert(docgen != NULL);
  assert(docgen->index_file != NULL);
  assert(docgen->type_file == NULL);
  assert(docgen->doc_dir != NULL);
  assert(ast != NULL);
  assert(package != NULL);

  AST_GET_CHILDREN(ast, id, tparams, cap, provides, members, c_api, doc);

  const char* name = ast_name(id);
  assert(name != NULL);

  // Build the type file name in a buffer.
  // Full file name is:
  //   doc_directory/docs/tqfn.md
  const char* sub_dir = "/docs/";
  const char* extn = ".md";
  size_t doc_dir_len = docgen->doc_dir_len;
  size_t sub_dir_len = strlen(sub_dir);
  size_t tqfn_len = tqfn_length(ast);
  size_t extn_len = strlen(extn);
  size_t sub_dir_start = doc_dir_len;
  size_t tqfn_start = sub_dir_start + sub_dir_len;
  size_t extn_start = tqfn_start + tqfn_len;
  size_t filename_len = extn_start + extn_len + 1;  // +1 for terminator

  char* buf = (char*)pool_alloc_size(filename_len);

  memcpy(buf, docgen->doc_dir, doc_dir_len);
  memcpy(buf + sub_dir_start, sub_dir, sub_dir_len);
  write_tqfn(ast, buf + tqfn_start);
  memcpy(buf + extn_start, extn, extn_len);
  buf[filename_len - 1] = '\0';

  // Now we have the file name open the file
  docgen->type_file = fopen(buf, "w");

  if(docgen->type_file == NULL)
  {
    ast_error(ast, "Could not write documentation to file %s", buf);
    pool_free_size(filename_len, buf);
    return;
  }

  // Add reference to new file to index file
  fprintf(docgen->index_file, "- [\"%.*s.md\", \"package %s\", \"%s %s\"]\n",
    (int)tqfn_len, buf + tqfn_start, package_qualified_name(package),
    ast_get_print(ast), name);

  pool_free_size(filename_len, buf);

  // Now we can write the actual documentation for the entity
  fprintf(docgen->type_file, "%s %s", ast_get_print(ast), name);
  doc_type_params(docgen, tparams);
  doc_type_list(docgen, provides, " is ", ", ", "");
  fprintf(docgen->type_file, "\n\nIn package \"%s\".\n\n",
    package_qualified_name(package));

  if(ast_id(doc) != TK_NONE)
    fprintf(docgen->type_file, "%s\n", ast_name(doc));

  fprintf(docgen->type_file, "%s, default capability %s.\n",
    (name[0] == '_') ? "Private" : "Public", ast_get_print(cap));

  if(ast_id(c_api) == TK_AT)
    fprintf(docgen->type_file, "May be called from C.\n");

  // Sort members into varieties
  ast_list_t pub_fields = { NULL, NULL, NULL };
  ast_list_t news = { NULL, NULL, NULL };
  ast_list_t bes = { NULL, NULL, NULL };
  ast_list_t funs = { NULL, NULL, NULL };

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    switch(ast_id(p))
    {
      case TK_FVAR:
      case TK_FLET:
        doc_list_add_named(&pub_fields, p, 0, true, false);
        break;

      case TK_NEW:
        doc_list_add_named(&news, p, 1, true, true);
        break;

      case TK_BE:
        doc_list_add_named(&bes, p, 1, true, true);
        break;

      case TK_FUN:
        doc_list_add_named(&funs, p, 1, true, true);
        break;

      default:
        assert(0);
        break;
    }
  }

  // Handle member variety lists
  doc_fields(docgen, &pub_fields, "Public fields");
  doc_methods(docgen, &news, "Constructors");
  doc_methods(docgen, &bes, "Behavious");
  doc_methods(docgen, &funs, "Functions");

  doc_list_free(&pub_fields);
  doc_list_free(&news);
  doc_list_free(&bes);
  doc_list_free(&funs);

  fclose(docgen->type_file);
  docgen->type_file = NULL;
}


// Document the given package
static void doc_package(docgen_t* docgen, ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_PACKAGE);

  ast_list_t types = { NULL, NULL, NULL };

  // Find and sort types
  for(ast_t* m = ast_child(ast); m != NULL; m = ast_sibling(m))
  {
    if(ast_id(m) == TK_STRING)
    {
      // Package docstring, TODO
      continue;
    }

    assert(ast_id(m) == TK_MODULE);

    for(ast_t* t = ast_child(m); t != NULL; t = ast_sibling(t))
    {
      if(ast_id(t) != TK_USE)
      {
        assert(ast_id(t) == TK_TYPE || ast_id(t) == TK_INTERFACE ||
          ast_id(t) == TK_TRAIT || ast_id(t) == TK_PRIMITIVE ||
          ast_id(t) == TK_CLASS || ast_id(t) == TK_ACTOR);
        // We have a type
        doc_list_add_named(&types, t, 0, true, true);
      }
    }
  }

  // Process types
  for(ast_list_t* p = types.next; p != NULL; p = p->next)
    doc_entity(docgen, p->ast, ast);
}


// Document the packages in the given program
static void doc_packages(docgen_t* docgen, ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_PROGRAM);

  // The Main package apepars first, other packages in alphabetical order
  ast_t* package_1 = NULL;

  ast_list_t packages = { NULL, NULL, NULL };

  // Sort packages
  for(ast_t* p = ast_child(ast); p != NULL; p = ast_sibling(p))
  {
    assert(ast_id(p) == TK_PACKAGE);

    if(strcmp(package_name(p), "$0") == 0)
    {
      assert(package_1 == NULL);
      package_1 = p;
    }
    else
    {
      const char* name = package_qualified_name(p);
      doc_list_add(&packages, p, name);
    }
  }

  // Process packages
  assert(package_1 != NULL);
  doc_package(docgen, package_1);

  for(ast_list_t* p = packages.next; p != NULL; p = p->next)
    doc_package(docgen, p->ast);
}


void generate_docs(ast_t* ast, const char* dir)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_PROGRAM);
  assert(dir != NULL);

  // Build the index file name in a buffer
  const char* filename = "/mkdocs.yml";
  size_t dir_len = strlen(dir);
  size_t filename_len = strlen(filename);
  size_t buf_len = dir_len + filename_len + 1;  // +1 for terminator

  char* buf = (char*)pool_alloc_size(buf_len);

  memcpy(buf, dir, dir_len);
  memcpy(buf + dir_len, filename, filename_len);
  buf[buf_len - 1] = '\0';

  // Now we have the file name open the file
  docgen_t docgen;
  docgen.index_file = fopen(buf, "w");

  if(docgen.index_file == NULL)
  {
    ast_error(ast, "Could not write documentation to file %s", buf);
    pool_free_size(buf_len, buf);
    return;
  }

  pool_free_size(buf_len, buf);

  fprintf(docgen.index_file, "site_name: Pony Generated Docs\n");
  fprintf(docgen.index_file, "pages:\n");

  // Process program
  docgen.doc_dir = dir;
  docgen.doc_dir_len = dir_len;
  docgen.type_file = NULL;
  doc_packages(&docgen, ast);

  fprintf(docgen.index_file, "theme: readthedocs\n");
  fclose(docgen.index_file);
}
