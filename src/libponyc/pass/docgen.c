#include "docgen.h"
#include "../pass/names.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>


// Define a type with the docgen state that needs to passed around the
// functions below.
// The doc directory is the root directory that we put our generated files
// into. This is supplied by the caller, presumably from a command line arg.
// Most of the time we have 3 files open:
// 1. The index file. This is the mkdocs yaml file that specifies the structure
// of the document and lives in the doc directory.
// This file is opened in the top level function before any processing occurs
// and is closed by the same function once we've processed everything.
// 2. The home file. This is the page that is used as the top level. It
// contains a contents list of the packages documented (with links).
// This file is opened in the top level function before any processing occurs
// and is closed by the same function once we've processed everything.
// 3. The type file. This is the md file for a specific type and lives in the
// "docs" sub directory of the doc directory. We have one file per type.
// This file is opened and closed by the functions that handles the type. When
// closed the file pointer should always be put back to NULL.
typedef struct docgen_t
{
  FILE* index_file;
  FILE* home_file;
  FILE* package_file;
  FILE* type_file;
  const char* base_dir;
  const char* sub_dir;
  size_t base_dir_buf_len;
  size_t sub_dir_buf_len;
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

  if(is_name_internal_test(name))  // Ignore internally generated names
    return;

  if(is_name_private(name) && !allow_private)  // Ignore private
    return;

  if(!is_name_private(name) && !allow_public)  // Ignore public
    return;

  if(is_name_private(name))  // Ignore leading underscore for ordering
    name++;

  doc_list_add(list, ast, name);
}


// Utilities

// Cat together the given strings into a newly allocated buffer.
// Any unneeded strings should be passed as "", not NULL.
// The returned buffer must be freed with ponyint_pool_free_size() when
// no longer needed.
// The out_buf_size parameter returns the size of the buffer (which is needed
// for freeing), not the length of the string.
static char* doc_cat(const char* a, const char* b, const char* c,
  const char* d, const char* e, size_t* out_buf_size)
{
  assert(a != NULL);
  assert(b != NULL);
  assert(c != NULL);
  assert(d != NULL);
  assert(e != NULL);
  assert(out_buf_size != NULL);

  size_t a_len = strlen(a);
  size_t b_len = strlen(b);
  size_t c_len = strlen(c);
  size_t d_len = strlen(d);
  size_t e_len = strlen(e);
  size_t buf_len = a_len + b_len + c_len + d_len + e_len + 1;

  char* buffer = (char*)ponyint_pool_alloc_size(buf_len);
  char *p = buffer;

  if(a_len > 0) { memcpy(p, a, a_len); p += a_len; }
  if(b_len > 0) { memcpy(p, b, b_len); p += b_len; }
  if(c_len > 0) { memcpy(p, c, c_len); p += c_len; }
  if(d_len > 0) { memcpy(p, d, d_len); p += d_len; }
  if(e_len > 0) { memcpy(p, e, e_len); p += e_len; }

  *(p++) = '\0';

  assert(p == (buffer + buf_len));
  *out_buf_size = buf_len;
  return buffer;
}


// Fully qualified type names (TQFNs).
// We need unique names for types, for use in file names and links. The format
// we use is:
//      qualfied_package_name-type_name
// All /s in the package name are replaced with dashes.
// This may fail if there are 2 or more packages with the same qualified name.
// This is unlikely, but possible and needs to be fixed later.

// Write the TQFN for the given type to a new buffer.
// By default the type name is taken from the given AST, however this can be
// overridden by the type_name parameter. Pass NULL to use the default.
// The returned buffer must be freed using ponyint_pool_free_size when no longer
// needed. Note that the size reported is the size of the buffer and includes a
// terminator.
static char* write_tqfn(ast_t* type, const char* type_name, size_t* out_size)
{
  assert(type != NULL);
  assert(out_size != NULL);

  ast_t* package = ast_nearest(type, TK_PACKAGE);
  assert(package != NULL);

  // We need the qualified package name and the type name
  const char* pkg_qual_name = package_qualified_name(package);

  if(type_name == NULL)
    type_name = ast_name(ast_child(type));

  assert(pkg_qual_name != NULL);
  assert(type_name != NULL);

  char* buffer = doc_cat(pkg_qual_name, "-", type_name, "", "", out_size);

  // Change slashes to dashes
  for(char* p = buffer; *p != '\0'; p++)
  {
    if(*p == '/')
      *p = '-';
#ifdef PLATFORM_IS_WINDOWS
    if(*p == '\\')
      *p = '-';
#endif
  }

  return buffer;
}


// Open a file with the specified info.
// The given filename extension should include a dot if one is needed.
// The returned file handle must be fclosed() with no longer needed.
// If the specified file cannot be opened an error will be generated and NULL
// returned.
static FILE* doc_open_file(docgen_t* docgen, bool in_sub_dir,
  const char* filename, const char* extn)
{
  assert(docgen != NULL);
  assert(filename != NULL);
  assert(extn != NULL);

  // Build the type file name in a buffer.
  // Full file name is:
  //   directory/filenameextn
  const char* dir = in_sub_dir ? docgen->sub_dir : docgen->base_dir;
  size_t buf_len;
  char* buffer = doc_cat(dir, filename, extn, "", "", &buf_len);

  // Now we have the file name open the file
  FILE* file = fopen(buffer, "w");

  if(file == NULL)
    errorf(NULL, "Could not write documentation to file %s", buffer);

  ponyint_pool_free_size(buf_len, buffer);
  return file;
}


// Functions to handle types

static void doc_type_list(docgen_t* docgen, ast_t* list, const char* preamble,
  const char* separator, const char* postamble);


// Report the human readable description for the given capability node.
// The returned string is valid forever and should not be freed.
// NULL is returned for no description.
static const char* doc_get_cap(ast_t* cap)
{
  if(cap == NULL)
    return NULL;

  switch(ast_id(cap))
  {
    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
    case TK_CAP_READ:
    case TK_CAP_SEND:
    case TK_CAP_SHARE:
      return ast_get_print(cap);

    default:
      return NULL;
  }
}


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

      size_t link_len;
      char* tqfn = write_tqfn(target, NULL, &link_len);

      // Links are of the form: [text](target)
      fprintf(docgen->type_file, "[%s](%s)", ast_name(id), tqfn);
      ponyint_pool_free_size(link_len, tqfn);

      doc_type_list(docgen, tparams, "\\[", ", ", "\\]");

      const char* cap_text = doc_get_cap(cap);
      if(cap_text != NULL)
        fprintf(docgen->type_file, " %s", cap_text);

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

      const char* cap_text = doc_get_cap(cap);
      if(cap_text != NULL)
        fprintf(docgen->type_file, " %s", cap_text);

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

    case TK_ISO:
    case TK_TRN:
    case TK_REF:
    case TK_VAL:
    case TK_BOX:
    case TK_TAG:
      fprintf(docgen->type_file, "%s", ast_get_print(type));
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

  fprintf(docgen->type_file, "## %s\n\n", title);

  for(ast_list_t* p = fields->next; p != NULL; p = p->next)
  {
    ast_t* field = p->ast;
    assert(field != NULL);

    AST_GET_CHILDREN(field, id, type, init);
    const char* name = ast_name(id);
    assert(name != NULL);

    // Don't want ast_get_print() as that will give us flet or fvar
    const char* ftype = NULL;

    switch(ast_id(field))
    {
      case TK_FVAR: ftype = "var"; break;
      case TK_FLET: ftype = "let"; break;
      case TK_EMBED: ftype = "embed"; break;
      default: assert(0);
    }

    fprintf(docgen->type_file, "* %s %s: ", ftype, name);
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
  //fprintf(docgen->type_file, "### %s %s()\n", ast_get_print(method), name);

  // Reconstruct signature for subheading
  fprintf(docgen->type_file, "### %s", ast_get_print(method));

  if(ast_id(method) == TK_FUN)
  {
    const char* cap_text = doc_get_cap(cap);
    if(cap_text != NULL)
      fprintf(docgen->type_file, " %s ", cap_text);
  }

  fprintf(docgen->type_file, " __%s__", name);
  doc_type_params(docgen, t_params);
  doc_params(docgen, params);

  if(ast_id(method) == TK_FUN)
  {
    fprintf(docgen->type_file, ": ");
    doc_type(docgen, ret);
  }

  if(ast_id(error) == TK_QUESTION)
    fprintf(docgen->type_file, " ?");

  fprintf(docgen->type_file, "\n\n");

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

  fprintf(docgen->type_file, "## %s\n\n", variety);

  for(ast_list_t* p = methods->next; p != NULL; p = p->next)
    doc_method(docgen, p->ast);
}


// Write a description of the given entity to its own type file.
// The containing package is handed in to save looking it up again.
static void doc_entity(docgen_t* docgen, ast_t* ast, ast_t* package)
{
  assert(docgen != NULL);
  assert(docgen->index_file != NULL);
  assert(docgen->package_file != NULL);
  assert(docgen->type_file == NULL);
  assert(ast != NULL);
  assert(package != NULL);

  // First open a file
  size_t tqfn_len;
  char* tqfn = write_tqfn(ast, NULL, &tqfn_len);

  docgen->type_file = doc_open_file(docgen, true, tqfn, ".md");

  if(docgen->type_file == NULL)
    return;

  // Add reference to new file to index file
  AST_GET_CHILDREN(ast, id, tparams, cap, provides, members, c_api, doc);

  const char* name = ast_name(id);
  assert(name != NULL);

  fprintf(docgen->index_file, "  - %s %s: \"%s.md\"\n",
    ast_get_print(ast), name, tqfn);

  fprintf(docgen->package_file, "* [%s %s](%s.md)\n",
    ast_get_print(ast), name, tqfn);

  ponyint_pool_free_size(tqfn_len, tqfn);

  // Now we can write the actual documentation for the entity
    fprintf(docgen->type_file, "# %s %s/%s",
    ast_get_print(ast),
    package_qualified_name(package),
    name);

  doc_type_params(docgen, tparams);
  doc_type_list(docgen, provides, " is ", ", ", "");
  fprintf(docgen->type_file, "\n\n");

  const char* cap_text = doc_get_cap(cap);
  if(cap_text != NULL)
    fprintf(docgen->type_file, "__Default capability__: _%s_\n\n", cap_text);

  if(ast_id(c_api) == TK_AT)
    fprintf(docgen->type_file, "May be called from C.\n");

  if(ast_id(doc) != TK_NONE)
    fprintf(docgen->type_file, "%s\n\n", ast_name(doc));
  else
    fprintf(docgen->type_file, "No doc string provided.\n\n");

  // Sort members into varieties
  ast_list_t pub_fields = { NULL, NULL, NULL };
  ast_list_t news = { NULL, NULL, NULL };
  ast_list_t pub_bes = { NULL, NULL, NULL };
  ast_list_t priv_bes = { NULL, NULL, NULL };
  ast_list_t pub_funs = { NULL, NULL, NULL };
  ast_list_t priv_funs = { NULL, NULL, NULL };

  for(ast_t* p = ast_child(members); p != NULL; p = ast_sibling(p))
  {
    switch(ast_id(p))
    {
      case TK_FVAR:
      case TK_FLET:
      case TK_EMBED:
        doc_list_add_named(&pub_fields, p, 0, true, false);
        break;

      case TK_NEW:
        doc_list_add_named(&news, p, 1, true, true);
        break;

      case TK_BE:
        doc_list_add_named(&pub_bes, p, 1, true, false);
        doc_list_add_named(&priv_bes, p, 1, false, true);
        break;

      case TK_FUN:
        doc_list_add_named(&pub_funs, p, 1, true, false);
        doc_list_add_named(&priv_funs, p, 1, false, true);
        break;

      default:
        assert(0);
        break;
    }
  }

  // Handle member variety lists
  doc_fields(docgen, &pub_fields, "Public fields");
  doc_methods(docgen, &news, "Constructors");
  doc_methods(docgen, &pub_bes, "Public Behaviours");
  doc_methods(docgen, &pub_funs, "Public Functions");
  doc_methods(docgen, &priv_bes, "Private Behaviours");
  doc_methods(docgen, &priv_funs, "Private Functions");

  doc_list_free(&pub_fields);
  doc_list_free(&news);
  doc_list_free(&pub_bes);
  doc_list_free(&priv_bes);
  doc_list_free(&pub_funs);
  doc_list_free(&priv_funs);

  fclose(docgen->type_file);
  docgen->type_file = NULL;
}


// Write the given package home page to its own file
static void doc_package_home(docgen_t* docgen,
  ast_t* package,
  ast_t* doc_string)
{
  assert(docgen != NULL);
  assert(docgen->index_file != NULL);
  assert(docgen->home_file != NULL);
  assert(docgen->package_file == NULL);
  assert(docgen->type_file == NULL);
  assert(package != NULL);
  assert(ast_id(package) == TK_PACKAGE);

  // First open a file
  size_t tqfn_len;
  char* tqfn = write_tqfn(package, "-index", &tqfn_len);

  // Package group
  fprintf(docgen->index_file, "- package %s:\n",
    package_qualified_name(package));

  docgen->type_file = doc_open_file(docgen, true, tqfn, ".md");

  if(docgen->type_file == NULL)
    return;

  // Add reference to new file to index file
  fprintf(docgen->index_file, "  - Package: \"%s.md\"\n", tqfn);

  // Add reference to package to home file
  fprintf(docgen->home_file, "* [%s](%s)\n", package_qualified_name(package),
    tqfn);

  // Now we can write the actual documentation for the package
  if(doc_string != NULL)
  {
    assert(ast_id(doc_string) == TK_STRING);
    fprintf(docgen->type_file, "%s", ast_name(doc_string));
  }
  else
  {
    fprintf(docgen->type_file, "No package doc string provided for %s.",
      package_qualified_name(package));
  }


  // Add listing of subpackages and links
  fprintf(docgen->type_file, "\n\n## Entities\n\n");

  ponyint_pool_free_size(tqfn_len, tqfn);

  docgen->package_file = docgen->type_file;
  docgen->type_file = NULL;
}


// Document the given package
static void doc_package(docgen_t* docgen, ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_PACKAGE);
  assert(docgen->package_file == NULL);

  ast_list_t types = { NULL, NULL, NULL };
  ast_t* package_doc = NULL;

  // Find and sort package contents
  for(ast_t* m = ast_child(ast); m != NULL; m = ast_sibling(m))
  {
    if(ast_id(m) == TK_STRING)
    {
      // Package docstring
      assert(package_doc == NULL);
      package_doc = m;
    }
    else
    {
      assert(ast_id(m) == TK_MODULE);

      for(ast_t* t = ast_child(m); t != NULL; t = ast_sibling(t))
      {
        if(ast_id(t) != TK_USE)
        {
          assert(ast_id(t) == TK_TYPE || ast_id(t) == TK_INTERFACE ||
            ast_id(t) == TK_TRAIT || ast_id(t) == TK_PRIMITIVE ||
            ast_id(t) == TK_STRUCT || ast_id(t) == TK_CLASS ||
            ast_id(t) == TK_ACTOR);
          // We have a type
          doc_list_add_named(&types, t, 0, true, true);
        }
      }
    }
  }

  doc_package_home(docgen, ast, package_doc);

  // Process types
  for(ast_list_t* p = types.next; p != NULL; p = p->next)
    doc_entity(docgen, p->ast, ast);

  fclose(docgen->package_file);
  docgen->package_file = NULL;
}


// Document the packages in the given program
static void doc_packages(docgen_t* docgen, ast_t* ast)
{
  assert(ast != NULL);
  assert(ast_id(ast) == TK_PROGRAM);

  // The Main package appears first, other packages in alphabetical order
  ast_t* package_1 = ast_child(ast);
  assert(package_1 != NULL);

  ast_list_t packages = { NULL, NULL, NULL };

  // Sort packages
  for(ast_t* p = ast_sibling(package_1); p != NULL; p = ast_sibling(p))
  {
    assert(ast_id(p) == TK_PACKAGE);

    const char* name = package_qualified_name(p);
    doc_list_add(&packages, p, name);
  }

  // Process packages
  docgen->package_file = NULL;
  doc_package(docgen, package_1);

  for(ast_list_t* p = packages.next; p != NULL; p = p->next)
    doc_package(docgen, p->ast);
}


// Delete all the files in the specified directory
static void doc_rm_star(const char* path)
{
  assert(path != NULL);

  PONY_ERRNO err;
  PONY_DIR* dir = pony_opendir(path, &err);

  if(dir == NULL)
    return;

  PONY_DIRINFO* result;

  while((result = pony_dir_entry_next(dir)) != NULL)
  {
    char* name = pony_dir_info_name(result);

    if(strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
    {
      // Delete this file
      size_t buf_len;
      char* buf = doc_cat(path, name, "", "", "", &buf_len);

#ifdef PLATFORM_IS_WINDOWS
      DeleteFile(buf);
#else
      remove(buf);
#endif
      ponyint_pool_free_size(buf_len, buf);
    }
  }

  pony_closedir(dir);
}


/* Ensure that the directories we need exist and are empty.
 *
 * Our base directory has the name:
 *    output/progname-docs/
 * where output/progname is the executable we are producing (without any
 * extension).
 *
 * Within this base directory we have the following:
 *    mkdocs.yml
 *    docs/
 *      *.md
 */
static void doc_setup_dirs(docgen_t* docgen, ast_t* program, pass_opt_t* opt)
{
  assert(docgen != NULL);
  assert(program != NULL);
  assert(opt != NULL);

  // First build our directory strings
  const char* output = opt->output;
  const char* progname = package_filename(ast_child(program));

  docgen->base_dir = doc_cat(output, "/", progname, "-docs/", "",
    &docgen->base_dir_buf_len);

  docgen->sub_dir = doc_cat(docgen->base_dir, "docs/", "", "", "",
    &docgen->sub_dir_buf_len);

  printf("Writing docs to %s\n", docgen->base_dir);

  // Create and clear out base directory
  pony_mkdir(docgen->base_dir);
  doc_rm_star(docgen->base_dir);

  // Create and clear out sub directory
  pony_mkdir(docgen->sub_dir);
  doc_rm_star(docgen->sub_dir);
}


void generate_docs(ast_t* program, pass_opt_t* options)
{
  assert(program != NULL);

  if(ast_id(program) != TK_PROGRAM)
    return;

  docgen_t docgen;
  doc_setup_dirs(&docgen, program, options);

  // Open the index and home files
  docgen.index_file = doc_open_file(&docgen, false, "mkdocs", ".yml");
  docgen.home_file = doc_open_file(&docgen, true, "index", ".md");
  docgen.type_file = NULL;

  // Write documentation files
  if(docgen.index_file != NULL && docgen.home_file != NULL)
  {
    ast_t* package = ast_child(program);
    const char* name = package_filename(package);

    fprintf(docgen.home_file, "Packages\n\n");

    fprintf(docgen.index_file, "site_name: %s\n", name);
    fprintf(docgen.index_file, "theme: readthedocs\n");
    fprintf(docgen.index_file, "pages:\n");
    fprintf(docgen.index_file, "- %s: index.md\n", name);

    doc_packages(&docgen, program);
  }

  // Tidy up
  if(docgen.index_file != NULL)
    fclose(docgen.index_file);

  if(docgen.home_file != NULL)
   fclose(docgen.home_file);

  if(docgen.base_dir != NULL)
    ponyint_pool_free_size(docgen.base_dir_buf_len, (void*)docgen.base_dir);

  if(docgen.sub_dir != NULL)
    ponyint_pool_free_size(docgen.sub_dir_buf_len, (void*)docgen.sub_dir);
}
