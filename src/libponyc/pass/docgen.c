#include "docgen.h"
#include "../ast/id.h"
#include "../ast/printbuf.h"
#include "../pkg/package.h"
#include "../../libponyrt/mem/pool.h"
#include "ponyassert.h"
#include <stdio.h>
#include <string.h>
#include "../common/paths.h"

// Type for storing the source code along the documentation.
typedef struct doc_sources_t
{
  const source_t* source; // The source file content
  const char* filename; // The source filename.
  size_t filename_alloc_size; // alloc size for the filename.
  const char* doc_path; // The relative path of the generated source file. Used for putting correct path in mkdocs.yml
  size_t doc_path_alloc_size; // alloc size for the doc_path.
  const char* file_path; // The absolute path of the generate source file.
  size_t file_path_alloc_size; // alloc size for the file_path.
  struct doc_sources_t* next; // The next element of the linked list.
} doc_sources_t;

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
  printbuf_t* public_types;
  printbuf_t* private_types;
  FILE* type_file;
  const char* base_dir;
  const char* sub_dir;
  const char* assets_dir;
  char* doc_source_dir;
  size_t base_dir_buf_len;
  size_t sub_dir_buf_len;
  size_t assets_dir_buf_len;
  errors_t* errors;
  doc_sources_t* included_sources; // As a linked list, being the first element or NULL
} docgen_t;

// Define options for doc generation
typedef struct docgen_opt_t
{
  bool include_private;
} docgen_opt_t;

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
  pony_assert(list != NULL);

  ast_list_t* p = list->next;

  while(p != NULL)
  {
    ast_list_t* next = p->next;
    POOL_FREE(ast_list_t, p);
    p = next;
  }
}

// Add the given AST to the given list, under the specified name
static void doc_list_add(ast_list_t* list, ast_t* ast, const char* name,
  bool sort)
{
  pony_assert(list != NULL);
  pony_assert(ast != NULL);

  ast_list_t* n = POOL_ALLOC(ast_list_t);
  n->ast = ast;
  n->name = name;
  n->next = NULL;


  // Find where to add name in sorted list
  ast_list_t* prev = list;

  for(ast_list_t* p = prev->next; p != NULL; prev = p, p = p->next)
  {
    pony_assert(p->name != NULL);

    if (sort && strcmp(p->name, name) > 0)
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

// detect if a whole package is for testing purposes
// statically checks if package name is "test" or "builtin_test"
static bool is_package_for_testing(const char* name)
{
    pony_assert(name != NULL);
    return (strcmp(name, "test") == 0 || strcmp(name, "builtin_test") == 0);
}

// Add the given AST to the given list, using the name from the specified
// child.
// ASTs with hygenic names are ignored.
// Leading underscores on names are ignored for sorting purposes.
// When false the allow_public parameter causes ASTs with public names to be
// ignored. allow_private does the same for private names.
static void doc_list_add_named(ast_list_t* list, ast_t* ast, size_t id_index,
  bool allow_public, bool allow_private, bool sort)
{
  pony_assert(list != NULL);
  pony_assert(ast != NULL);

  if(ast_has_annotation(ast, "nodoc"))
    return;

  const char* name = ast_name(ast_childidx(ast, id_index));
  pony_assert(name != NULL);

  if(is_name_internal_test(name))  // Ignore internally generated names
    return;

  bool has_private_name = is_name_private(name);

  if(has_private_name && !allow_private)  // Ignore private
    return;


  if(!has_private_name && !allow_public)  // Ignore public
    return;

  if(has_private_name)  // Ignore leading underscore for ordering
    name++;

  doc_list_add(list, ast, name, sort);
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
  pony_assert(a != NULL);
  pony_assert(b != NULL);
  pony_assert(c != NULL);
  pony_assert(d != NULL);
  pony_assert(e != NULL);
  pony_assert(out_buf_size != NULL);

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

  pony_assert(p == (buffer + buf_len));
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
  pony_assert(type != NULL);
  pony_assert(out_size != NULL);

  ast_t* package = ast_nearest(type, TK_PACKAGE);
  pony_assert(package != NULL);

  // We need the qualified package name and the type name
  const char* pkg_qual_name = package_qualified_name(package);

  if(type_name == NULL)
    type_name = ast_name(ast_child(type));

  pony_assert(pkg_qual_name != NULL);
  pony_assert(type_name != NULL);

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

static FILE* doc_open_file_(docgen_t* docgen, const char* dir,
  const char* filename, const char* extn, bool binary)
{
  pony_assert(docgen != NULL);
  pony_assert(filename != NULL);
  pony_assert(extn != NULL);

  // Build the type file name in a buffer.
  // Full file name is:
  //   directory/filenameextn
  size_t buf_len;
  char* buffer = doc_cat(dir, filename, extn, "", "", &buf_len);

  // Now we have the file name open the file
  const char* mode = binary ? "wb" : "w";
  FILE* file = fopen(buffer, mode);

  if(file == NULL)
    errorf(docgen->errors, NULL,
      "Could not write documentation to file %s", buffer);

  ponyint_pool_free_size(buf_len, buffer);
  return file;
}

// Open a file with the specified info.
// The given filename extension should include a dot if one is needed.
// The returned file handle must be fclosed() with no longer needed.
// If the specified file cannot be opened an error will be generated and NULL
// returned.
static FILE* doc_open_file(docgen_t* docgen, const char* dir, const char* filename, const char* extn) {
  return doc_open_file_(docgen, dir, filename, extn, false);
}

static FILE* doc_open_binary_file(docgen_t* docgen, const char* dir, const char* filename, const char* extn) {
  return doc_open_file_(docgen, dir, filename, extn, true);
}

// Functions to handle types

static void doc_type_list(docgen_t* docgen, docgen_opt_t* docgen_opt, ast_t* list,
  const char* preamble, const char* separator, const char* postamble,
  bool generate_links, bool break_lines);

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

static const doc_sources_t* get_doc_source(docgen_t* docgen, source_t* source)
{
  if (docgen->included_sources == NULL)
    return NULL;

  const doc_sources_t* current_elem = docgen->included_sources;

  while (current_elem != NULL) {
    if (source == current_elem->source)
      return current_elem;
    current_elem = current_elem->next;
  }
  return NULL;
}

// Write the given type to the current type file
static void doc_type(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_t* type, bool generate_links, bool break_lines)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(type != NULL);

  switch(ast_id(type))
  {
    case TK_NOMINAL:
    {
      AST_GET_CHILDREN(type, package, id, tparams, cap, ephemeral);

      // Generate links only if directed to and if the type is not anonymous (as
      // indicated by a name created by package_hygienic_id)
      // and if the type not private if we exclude private types.
      const char* type_id_name = ast_name(id);
      if(generate_links && *type_id_name != '$'
        && (docgen_opt->include_private || !is_name_private(type_id_name)))
      {
        // Find type we reference so we can link to it
        ast_t* target = (ast_t*)ast_data(type);
        pony_assert(target != NULL);

        size_t link_len;
        char* tqfn = write_tqfn(target, NULL, &link_len);

        // Links are of the form: [text](target)
        // mkdocs requires the full filename for creating proper links
        fprintf(docgen->type_file, "[%s](%s.md)", ast_nice_name(id), tqfn);
        ponyint_pool_free_size(link_len, tqfn);

        doc_type_list(docgen, docgen_opt, tparams, "\\[", ", ", "\\]", true, false);
      }
      else
      {
        fprintf(docgen->type_file, "%s", ast_nice_name(id));
        doc_type_list(docgen, docgen_opt, tparams, "[", ", ", "]", false, false);
      }

      const char* cap_text = doc_get_cap(cap);

      if(cap_text != NULL)
        fprintf(docgen->type_file, " %s", cap_text);

      if(ast_id(ephemeral) != TK_NONE)
        fprintf(docgen->type_file, "%s", ast_get_print(ephemeral));

      break;
    }

    case TK_UNIONTYPE:
      doc_type_list(docgen, docgen_opt, type, "(", " | ", ")", generate_links, break_lines);
      break;

    case TK_ISECTTYPE:
      doc_type_list(docgen, docgen_opt, type, "(", " & ", ")", generate_links, break_lines);
      break;

    case TK_TUPLETYPE:
      doc_type_list(docgen, docgen_opt, type, "(", " , ", ")", generate_links, break_lines);
      break;

    case TK_TYPEPARAMREF:
    {
      AST_GET_CHILDREN(type, id, cap, ephemeral);
      fprintf(docgen->type_file, "%s", ast_nice_name(id));

      if(ast_id(ephemeral) != TK_NONE)
        fprintf(docgen->type_file, "%s", ast_get_print(ephemeral));

      break;
    }

    case TK_ARROW:
    {
      AST_GET_CHILDREN(type, left, right);
      doc_type(docgen, docgen_opt, left, generate_links, break_lines);
      fprintf(docgen->type_file, "->");
      doc_type(docgen, docgen_opt, right, generate_links, break_lines);
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
      pony_assert(0);
  }
}

// Write the given list of types to the current type file, with the specified
// preamble, separator and postamble text. If the list is empty nothing is
// written.
static void doc_type_list(docgen_t* docgen, docgen_opt_t* docgen_opt, ast_t* list,
  const char* preamble, const char* separator, const char* postamble,
  bool generate_links, bool break_lines)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(list != NULL);
  pony_assert(preamble != NULL);
  pony_assert(separator != NULL);
  pony_assert(postamble != NULL);

  if(ast_id(list) == TK_NONE)
    return;

  fprintf(docgen->type_file, "%s", preamble);

  int listItemCount = 0;
  for(ast_t* p = ast_child(list); p != NULL; p = ast_sibling(p))
  {
    doc_type(docgen, docgen_opt, p, generate_links, break_lines);

    if(ast_sibling(p) != NULL) {
      fprintf(docgen->type_file, "%s", separator);

      if (break_lines) {
        if (listItemCount++ == 2) {
          fprintf(docgen->type_file, "\n    ");
          listItemCount = 0;
        }
      }
    }

  }

  fprintf(docgen->type_file, "%s", postamble);
}

static void add_source_code_link(docgen_t* docgen, ast_t* elem)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  source_t* source = ast_source(elem);
  const doc_sources_t* doc_source = NULL;

  if (source != NULL)
    doc_source = get_doc_source(docgen, source);

  if (doc_source != NULL) {
      fprintf(
        docgen->type_file,
        "\n<span class=\"source-link\">[[Source]](%s#L-0-%zd)</span>\n",
        doc_source->doc_path, ast_line(elem)
      );
  } else {
    fprintf(docgen->type_file, "\n\n");
  }
}

// Functions to handle everything else

// Write the given list of fields to the current type file.
// The given title text is used as a section header.
// If the field list is empty nothing is written.
static void doc_fields(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_list_t* fields, const char* title)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(fields != NULL);
  pony_assert(title != NULL);

  if(fields->next == NULL)  // No fields
    return;

  fprintf(docgen->type_file, "## %s\n\n", title);

  for(ast_list_t* p = fields->next; p != NULL; p = p->next)
  {
    ast_t* field = p->ast;
    pony_assert(field != NULL);

    AST_GET_CHILDREN(field, id, type, init, doc);
    const char* name = ast_name(id);
    pony_assert(name != NULL);

    // Don't want ast_get_print() as that will give us flet or fvar
    const char* ftype = NULL;

    switch(ast_id(field))
    {
      case TK_FVAR: ftype = "var"; break;
      case TK_FLET: ftype = "let"; break;
      case TK_EMBED: ftype = "embed"; break;
      default: pony_assert(0);
    }

    fprintf(docgen->type_file, "### %s %s: ", ftype, name);
    doc_type(docgen, docgen_opt, type, true, false);
    add_source_code_link(docgen, field);
    fprintf(docgen->type_file, "\n");

    if(ast_id(doc) != TK_NONE)
      fprintf(docgen->type_file, "%s\n\n", ast_name(doc));

    fprintf(docgen->type_file, "\n\n---\n\n");
  }
}

// Write the given list of type parameters to the current type file, with
// surrounding []. If the given list is empty nothing is written.
static void doc_type_params(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_t* t_params, bool generate_links, bool break_lines)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(t_params != NULL);

  if(ast_id(t_params) == TK_NONE)
    return;

  pony_assert(ast_id(t_params) == TK_TYPEPARAMS);

  if(generate_links)
    fprintf(docgen->type_file, "\\[");
  else
    fprintf(docgen->type_file, "[");
  ast_t* first = ast_child(t_params);

  for(ast_t* t_param = first; t_param != NULL; t_param = ast_sibling(t_param))
  {
    if(t_param != first)
      fprintf(docgen->type_file, ", ");

    AST_GET_CHILDREN(t_param, id, constraint, default_type);
    const char* name = ast_name(id);
    pony_assert(name != NULL);

    if(ast_id(default_type) != TK_NONE)
      fprintf(docgen->type_file, "optional ");

    fprintf(docgen->type_file, "%s: ", name);

    if(ast_id(constraint) != TK_NONE)
      doc_type(docgen, docgen_opt, constraint, generate_links, break_lines);
    else
      fprintf(docgen->type_file, "no constraint");
  }

  if(generate_links)
    fprintf(docgen->type_file, "\\]");
  else
    fprintf(docgen->type_file, "]");
}

// Write the given list of parameters to the current type file, with
// surrounding (). If the given list is empty () is still written.
static void code_block_doc_params(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_t* params)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(params != NULL);

  fprintf(docgen->type_file, "(");
  ast_t* first = ast_child(params);

  for(ast_t* param = first; param != NULL; param = ast_sibling(param))
  {
    if(param != first)
      fprintf(docgen->type_file, ",\n");
    else
      fprintf(docgen->type_file, "\n");

    AST_GET_CHILDREN(param, id, type, def_val);
    const char* name = ast_name(id);
    pony_assert(name != NULL);

    fprintf(docgen->type_file, "  %s: ", name);
    doc_type(docgen, docgen_opt, type, false, true);

    // if we have a default value, add it to the documentation
    if(ast_id(def_val) != TK_NONE)
    {
      switch(ast_id(ast_child(def_val)))
      {
        case TK_STRING:
          fprintf(docgen->type_file, " = \"%s\"", ast_get_print(ast_child(def_val)));
          break;

        default:
          fprintf(docgen->type_file, " = %s", ast_get_print(ast_child(def_val)));
          break;
      }
    }
  }

  fprintf(docgen->type_file, ")");
}

static void list_doc_params(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_t* params)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(params != NULL);

  ast_t* first = ast_child(params);

  for(ast_t* param = first; param != NULL; param = ast_sibling(param))
  {
    if(param == first)
      fprintf(docgen->type_file, "#### Parameters\n\n");

    fprintf(docgen->type_file, "* ");

    AST_GET_CHILDREN(param, id, type, def_val);
    const char* name = ast_name(id);
    pony_assert(name != NULL);

    fprintf(docgen->type_file, "  %s: ", name);
    doc_type(docgen, docgen_opt, type, true, true);

    // if we have a default value, add it to the documentation
    if(ast_id(def_val) != TK_NONE)
    {
      switch(ast_id(ast_child(def_val)))
      {
        case TK_STRING:
          fprintf(docgen->type_file, " = \"%s\"", ast_get_print(ast_child(def_val)));
          break;

        default:
          fprintf(docgen->type_file, " = %s", ast_get_print(ast_child(def_val)));
          break;
      }
    }

    fprintf(docgen->type_file, "\n");
  }

  fprintf(docgen->type_file, "\n");
}

// Write a description of the given method to the current type file
static void doc_method(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_t* method)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(method != NULL);

  AST_GET_CHILDREN(method, cap, id, t_params, params, ret, error, body, doc);

  const char* name = ast_name(id);
  pony_assert(name != NULL);

  // Method
  fprintf(docgen->type_file, "### %s", name);
  doc_type_params(docgen, docgen_opt, t_params, true, false);

  add_source_code_link(docgen, method);

  fprintf(docgen->type_file, "\n\n");

  // The docstring, if any
  if(ast_id(doc) != TK_NONE)
    fprintf(docgen->type_file, "%s\n\n", ast_name(doc));

  // SYLVAN'S FULL CODE BLOCK HERE
  fprintf(docgen->type_file, "```pony\n");
  fprintf(docgen->type_file, "%s ", ast_get_print(method));
  if(ast_id(method) == TK_FUN || ast_id(method) == TK_NEW)
  {
    const char* cap_text = doc_get_cap(cap);
    if(cap_text != NULL) fprintf(docgen->type_file, "%s ", cap_text);
  }
  fprintf(docgen->type_file, "%s", name);
  doc_type_params(docgen, docgen_opt, t_params, false, true);
  // parameters of the code block
  code_block_doc_params(docgen, docgen_opt, params);

  // return type
  if(ast_id(method) == TK_FUN || ast_id(method) == TK_NEW)
  {
    fprintf(docgen->type_file, "\n: ");
    doc_type(docgen, docgen_opt, ret, false, true);

    if(ast_id(error) == TK_QUESTION)
      fprintf(docgen->type_file, " ?");
  }

  // close the block
  fprintf(docgen->type_file, "\n```\n");

  // Parameters
  list_doc_params(docgen, docgen_opt, params);

  // Return value
  if(ast_id(method) == TK_FUN || ast_id(method) == TK_NEW)
  {
    fprintf(docgen->type_file, "#### Returns\n\n");
    fprintf(docgen->type_file, "* ");
    doc_type(docgen, docgen_opt, ret, true, true);

    if(ast_id(error) == TK_QUESTION)
      fprintf(docgen->type_file, " ?");

    fprintf(docgen->type_file, "\n\n");
  }

  // horizontal rule at the end
  // separate us from the next method visually
  fprintf(docgen->type_file, "---\n\n");
}

// Write the given list of methods to the current type file.
// The variety text is used as a heading.
// If the list is empty nothing is written.
static void doc_methods(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_list_t* methods, const char* variety)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->type_file != NULL);
  pony_assert(methods != NULL);
  pony_assert(variety != NULL);

  if(methods->next == NULL)
    return;

  fprintf(docgen->type_file, "## %s\n\n", variety);

  for(ast_list_t* p = methods->next; p != NULL; p = p->next)
    doc_method(docgen, docgen_opt, p->ast);
}

static char* concat(const char *s1, const char *s2, size_t* allocated_size)
{
  size_t str_size = strlen(s1) + strlen(s2) + 1;
  char* result = (char*) ponyint_pool_alloc_size(str_size); //+1 for the null-terminator
  *allocated_size = str_size;
  strcpy(result, s1);
  strcat(result, s2);
  result[str_size - 1] = '\0';
  return result;
}

static doc_sources_t* copy_source_to_doc_src(docgen_t* docgen, source_t* source, const char* package_name)
{
  pony_assert(docgen != NULL);
  pony_assert(source != NULL);
  pony_assert(package_name != NULL);

  doc_sources_t* result = (doc_sources_t*) ponyint_pool_alloc_size(sizeof(doc_sources_t));

  char filename_copy[FILENAME_MAX];
  strcpy(filename_copy, source->file);

  const char* just_filename = get_file_name(filename_copy);
  size_t filename_alloc_size = strlen(just_filename) + 1;
  char* filename = (char*) ponyint_pool_alloc_size(filename_alloc_size);
  strcpy(filename, just_filename);
  size_t filename_without_ext_alloc_size = 0;
  const char* filename_without_ext = remove_ext(filename, '.', 0, &filename_without_ext_alloc_size);
  size_t filename_md_extension_alloc_size = 0;
  const char* filename_md_extension = concat(filename_without_ext, ".md", &filename_md_extension_alloc_size);

  // Absolute path where a copy of the source will be put.
  char source_dir[FILENAME_MAX];
  path_cat(docgen->doc_source_dir, package_name, source_dir);

  //Create directory for [documentationDir]/src/[package_name]
  pony_mkdir(source_dir);

  // Get absolute path for [documentationDir]/src/[package_name]/[filename].md
  size_t file_path_alloc_size = FILENAME_MAX;
  char* path = (char*) ponyint_pool_alloc_size(FILENAME_MAX);
  path_cat(source_dir, filename_md_extension, path);

  // Get relative path for [documentationDir]/src/[package_name]/
  // so it can be written in the mkdocs.yml file.

  size_t old_ptr_alloc_size = 0;
  const char* doc_source_dir_relative = concat("src/", package_name, &old_ptr_alloc_size);
  const char* old_ptr = doc_source_dir_relative;
  size_t doc_source_dir_relative_alloc_size = 0;
  doc_source_dir_relative = concat(doc_source_dir_relative, "/", &doc_source_dir_relative_alloc_size);
  ponyint_pool_free_size(old_ptr_alloc_size, (void*) old_ptr);

  // Get relative path for [documentationDir]/src/[package_name]/[filename].md
  size_t doc_path_alloc_size = 0;
  const char* doc_path = concat(doc_source_dir_relative, filename_md_extension, &doc_path_alloc_size);

  ponyint_pool_free_size(filename_without_ext_alloc_size, (void*) filename_without_ext);
  ponyint_pool_free_size(filename_md_extension_alloc_size, (void*) filename_md_extension);
  ponyint_pool_free_size(doc_source_dir_relative_alloc_size, (void*) doc_source_dir_relative);

  // Section to copy source file to [documentationDir]/src/[package_name]/[filename].md
  FILE* file = fopen(path, "w");

  if (file != NULL) {
    // start front matter
    fprintf(file, "---\n");
    // tell the mkdocs theme to hide the right sidebar
    // so we have all the space we need for long lines
    fprintf(file, "hide:\n");
    fprintf(file, "  - toc\n");
    // tell the mkdocs theme not index source files for search
    fprintf(file, "search:\n");
    fprintf(file, "  exclude: true\n");
    // end front matter
    fprintf(file, "---\n");
    // Escape markdown to tell this is Pony code
    // Using multiple '```````'  so hopefully the markdown parser
    // will consider the whole text as a code block.
    // add line nums as well
    fprintf(file, "```````pony linenums=\"1\"\n");
    fprintf(file, "%s", source->m);
    fprintf(file, "\n```````");
    fclose(file);

    result->source = source;

    result->filename = filename;
    result->filename_alloc_size= filename_alloc_size;
    result->doc_path = doc_path;
    result->doc_path_alloc_size = doc_path_alloc_size;
    result->file_path = path;
    result->file_path_alloc_size = file_path_alloc_size;

    result->next = NULL;

    return result;
  } else {
    errorf(docgen->errors, NULL, "Could not write source-file to %s", path);
    ponyint_pool_free_size(filename_alloc_size, (void*) filename);
    ponyint_pool_free_size(doc_path_alloc_size, (void*) doc_path);
    ponyint_pool_free_size(file_path_alloc_size, (void*) path);
    return NULL;
  }
}

static char* replace_path_separator(const char* path, size_t* name_len) {
  size_t str_len = strlen(path);
  *name_len = str_len + 1;
  char* buffer = (char*) ponyint_pool_alloc_size(*name_len);
  memcpy(buffer, path, str_len);
  for(char* p = buffer; *p != '\0'; p++)
  {
    if(*p == '.')
      *p = '_';
    if(*p == '/')
      *p = '-';
#ifdef PLATFORM_IS_WINDOWS
    if(*p == '\\')
      *p = '-';
#endif
  }
  buffer[str_len] = '\0';
  return buffer;
}

static void include_source_if_needed(
  docgen_t* docgen,
  source_t* source,
  const char* package_name
)
{
  pony_assert(source != NULL);
  pony_assert(docgen != NULL);
  const char* source_path = source->file;
  pony_assert(source_path != NULL);

  if (docgen->included_sources == NULL) {
    docgen->included_sources = copy_source_to_doc_src(docgen, source, package_name);
  } else {
    doc_sources_t* current_source = docgen->included_sources;
    doc_sources_t* last_valid_source = current_source;
    bool is_already_included = false;
    while (current_source != NULL && !is_already_included) {
      pony_assert(current_source != NULL);
      pony_assert(current_source->source != NULL);
      pony_assert(current_source->source->file != NULL);

      if(strcmp(source_path, current_source->source->file) == 0) {
        is_already_included = true;
      } else {
        last_valid_source = current_source;
        current_source = current_source->next;
      }
    }
    if (!is_already_included) {
      last_valid_source->next = copy_source_to_doc_src(docgen, source, package_name);
    }
  }
}

// Write a description of the given entity to its own type file.
static void doc_entity(docgen_t* docgen, docgen_opt_t* docgen_opt, ast_t* ast)
{
  pony_assert(docgen != NULL);
  pony_assert(docgen->index_file != NULL);
  pony_assert(docgen->package_file != NULL);
  pony_assert(docgen->public_types != NULL);
  pony_assert(docgen->private_types != NULL);
  pony_assert(docgen->type_file == NULL);
  pony_assert(ast != NULL);

  ast_t* package =  ast;
  while (ast_parent(package) != NULL && ast_id(package) != TK_PACKAGE)
  {
    package = ast_parent(package);
  }

  size_t package_name_len;

  char* package_name =
    replace_path_separator(
      package_qualified_name(package),
      &package_name_len
    );

  source_t* source = ast_source(ast);
  if (source != NULL && package != NULL)
    include_source_if_needed(docgen, source, package_name);

  ponyint_pool_free_size(package_name_len, package_name);

  // First open a file
  size_t tqfn_len;
  char* tqfn = write_tqfn(ast, NULL, &tqfn_len);

  docgen->type_file = doc_open_file(docgen, docgen->sub_dir, tqfn, ".md");

  if(docgen->type_file == NULL)
    return;

  // Add reference to new file to index file
  AST_GET_CHILDREN(ast, id, tparams, cap, provides, members, c_api, doc);

  const char* name = ast_name(id);
  pony_assert(name != NULL);

  fprintf(docgen->index_file, "  - %s %s: \"%s.md\"\n",
    ast_get_print(ast), name, tqfn);

  // Add to appropriate package types buffer
  printbuf_t* buffer = docgen->public_types;
  if(is_name_private(name)) buffer = docgen->private_types;
  printbuf(buffer,
           "* [%s %s](%s.md)\n",
           ast_get_print(ast), name, tqfn);

  ponyint_pool_free_size(tqfn_len, tqfn);

  // Now we can write the actual documentation for the entity
  fprintf(docgen->type_file, "# %s", name);

  doc_type_params(docgen, docgen_opt, tparams, true, false);

  add_source_code_link(docgen, ast);

  if(ast_id(doc) != TK_NONE)
    // additional linebreak for better source code link display with docstring
    fprintf(docgen->type_file, "\n%s\n\n", ast_name(doc));

  // code block
  fprintf(docgen->type_file, "```pony\n");
  fprintf(docgen->type_file, "%s ",ast_get_print(ast));

  const char* cap_text = doc_get_cap(cap);
  if(cap_text != NULL) fprintf(docgen->type_file, "%s ", cap_text);

  fprintf(docgen->type_file, "%s", name);

  doc_type_params(docgen, docgen_opt, tparams, false, true);
  doc_type_list(docgen, docgen_opt, provides, " is\n  ", ",\n  ", "",
    false, false);
  fprintf(docgen->type_file, "\n```\n\n");

  if (ast_id(ast) !=  TK_TYPE)
    doc_type_list(docgen, docgen_opt, provides,
      "#### Implements\n\n* ", "\n* ", "\n\n---\n\n", true, false);
  else
    doc_type_list(docgen, docgen_opt, provides,
      "#### Type Alias For\n\n* ", "\n* ", "\n\n---\n\n", true, false);

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
        doc_list_add_named(&pub_fields, p, 0, true, false, false);
        break;

      case TK_NEW:
        doc_list_add_named(&news, p, 1, true, docgen_opt->include_private, false);
        break;

      case TK_BE:
        doc_list_add_named(&pub_bes, p, 1, true, false, false);
        doc_list_add_named(&priv_bes, p, 1,
          false, docgen_opt->include_private, false);
        break;

      case TK_FUN:
        doc_list_add_named(&pub_funs, p, 1, true, false, false);
        doc_list_add_named(&priv_funs, p, 1,
          false, docgen_opt->include_private, false);
        break;

      default:
        pony_assert(0);
        break;
    }
  }

  // Handle member variety lists
  doc_methods(docgen, docgen_opt, &news, "Constructors");
  doc_fields(docgen, docgen_opt, &pub_fields, "Public fields");
  doc_methods(docgen, docgen_opt, &pub_bes, "Public Behaviours");
  doc_methods(docgen, docgen_opt, &pub_funs, "Public Functions");
  doc_methods(docgen, docgen_opt, &priv_bes, "Private Behaviours");
  doc_methods(docgen, docgen_opt, &priv_funs, "Private Functions");

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
  pony_assert(docgen != NULL);
  pony_assert(docgen->index_file != NULL);
  pony_assert(docgen->home_file != NULL);
  pony_assert(docgen->package_file == NULL);
  pony_assert(docgen->public_types == NULL);
  pony_assert(docgen->private_types == NULL);
  pony_assert(docgen->type_file == NULL);
  pony_assert(package != NULL);
  pony_assert(ast_id(package) == TK_PACKAGE);

  // First open a file
  size_t tqfn_len;
  char* tqfn = write_tqfn(package, "-index", &tqfn_len);

  // Package group
  fprintf(docgen->index_file, "- package %s:\n",
    package_qualified_name(package));

  docgen->type_file = doc_open_file(docgen, docgen->sub_dir, tqfn, ".md");

  if(docgen->type_file == NULL)
    return;

  // Add reference to new file to index file
  fprintf(docgen->index_file, "  - Package: \"%s.md\"\n", tqfn);

  // Add reference to package to home file
  fprintf(docgen->home_file, "* [%s](%s.md)\n", package_qualified_name(package),
    tqfn);

  // Now we can write the actual documentation for the package
  if(doc_string != NULL)
  {
    pony_assert(ast_id(doc_string) == TK_STRING);
    fprintf(docgen->type_file, "%s", ast_name(doc_string));
  }
  else
  {
    fprintf(docgen->type_file, "No package doc string provided for %s.",
      package_qualified_name(package));
  }


  ponyint_pool_free_size(tqfn_len, tqfn);

  docgen->public_types = printbuf_new();
  docgen->private_types = printbuf_new();

  docgen->package_file = docgen->type_file;
  docgen->type_file = NULL;
}

// Document the given package
static void doc_package(docgen_t* docgen, docgen_opt_t* docgen_opt, ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(ast_id(ast) == TK_PACKAGE);
  pony_assert(docgen->package_file == NULL);
  pony_assert(docgen->public_types == NULL);
  pony_assert(docgen->private_types == NULL);

  ast_list_t types = { NULL, NULL, NULL };
  ast_t* package_doc = NULL;

  // Find and sort package contents
  for(ast_t* m = ast_child(ast); m != NULL; m = ast_sibling(m))
  {
    if(ast_id(m) == TK_STRING)
    {
      // Package docstring
      pony_assert(package_doc == NULL);
      package_doc = m;
    }
    else
    {
      pony_assert(ast_id(m) == TK_MODULE);

      for(ast_t* t = ast_child(m); t != NULL; t = ast_sibling(t))
      {
        if(ast_id(t) != TK_USE)
        {
          pony_assert(ast_id(t) == TK_TYPE || ast_id(t) == TK_INTERFACE ||
            ast_id(t) == TK_TRAIT || ast_id(t) == TK_PRIMITIVE ||
            ast_id(t) == TK_STRUCT || ast_id(t) == TK_CLASS ||
            ast_id(t) == TK_ACTOR);
          // We have a type
          doc_list_add_named(&types, t, 0, true, docgen_opt->include_private, true);
        }
      }
    }
  }

  doc_package_home(docgen, ast, package_doc);

  // Process types
  for(ast_list_t* p = types.next; p != NULL; p = p->next)
    doc_entity(docgen, docgen_opt, p->ast);

  // Add listing of subpackages and links
  if(docgen->public_types->offset > 0)
  {
    fprintf(docgen->package_file, "\n\n## Public Types\n\n");
    fprintf(docgen->package_file, "%s", docgen->public_types->m);
  }

  if(docgen_opt->include_private && docgen->private_types->offset > 0)
  {
    fprintf(docgen->package_file, "\n\n## Private Types\n\n");
    fprintf(docgen->package_file, "%s", docgen->private_types->m);
  }

  fclose(docgen->package_file);
  docgen->package_file = NULL;
  printbuf_free(docgen->public_types);
  printbuf_free(docgen->private_types);
  docgen->public_types = NULL;
  docgen->private_types = NULL;
}


// Document the packages in the given program
static void doc_packages(docgen_t* docgen, docgen_opt_t* docgen_opt,
  ast_t* ast)
{
  pony_assert(ast != NULL);
  pony_assert(ast_id(ast) == TK_PROGRAM);

  // The Main package appears first, other packages in alphabetical order
  ast_t* package_1 = ast_child(ast);
  pony_assert(package_1 != NULL);

  ast_list_t packages = { NULL, NULL, NULL };

  // Sort packages
  for(ast_t* p = ast_sibling(package_1); p != NULL; p = ast_sibling(p))
  {
    pony_assert(ast_id(p) == TK_PACKAGE);

    const char* name = package_qualified_name(p);
    doc_list_add(&packages, p, name, true);
  }

  // Process packages
  docgen->package_file = NULL;
  docgen->public_types = NULL;
  docgen->private_types = NULL;
  doc_package(docgen, docgen_opt, package_1);

  for(ast_list_t* p = packages.next; p != NULL; p = p->next)
  {
    const char* p_name = package_qualified_name(p->ast);
    if(!is_package_for_testing(p_name))
    {
      doc_package(docgen, docgen_opt, p->ast);
    }
  }
}

// Delete all the files in the specified directory
static void doc_rm_star(const char* path)
{
  pony_assert(path != NULL);

  PONY_ERRNO err;
  PONY_DIR* dir = pony_opendir(path, &err);

  if(dir == NULL)
  {
    printf("Couldn't open %s", path);
    return;
  }

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
  pony_assert(docgen != NULL);
  pony_assert(program != NULL);
  pony_assert(opt != NULL);

  // First build our directory strings
  const char* output = opt->output;
  const char* progname = package_filename(ast_child(program));

  docgen->base_dir = doc_cat(output, "/", progname, "-docs/", "",
    &docgen->base_dir_buf_len);

  docgen->sub_dir = doc_cat(docgen->base_dir, "docs/", "", "", "",
    &docgen->sub_dir_buf_len);

  docgen->assets_dir = doc_cat(docgen->sub_dir, PONYLANG_MKDOCS_ASSETS_DIR, "", "", "",
    &docgen->assets_dir_buf_len);

  docgen->doc_source_dir = (char*) ponyint_pool_alloc_size(FILENAME_MAX);
  path_cat(docgen->base_dir, "docs/src", docgen->doc_source_dir);

  if(opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, "Writing docs to %s\n", docgen->base_dir);

  // Create and clear out base directory
  pony_mkdir(docgen->base_dir);
  doc_rm_star(docgen->base_dir);

  // Create and clear out source directory
  pony_mkdir(docgen->doc_source_dir);
  doc_rm_star(docgen->doc_source_dir);

  // Create and clear out sub directory
  pony_mkdir(docgen->sub_dir);
  doc_rm_star(docgen->sub_dir);

  // Create extra css directory (within sub_dir)
  pony_mkdir(docgen->assets_dir);
  doc_rm_star(docgen->assets_dir);
}

void generate_docs(ast_t* program, pass_opt_t* options)
{
  pony_assert(program != NULL);

  if(ast_id(program) != TK_PROGRAM)
    return;

  docgen_opt_t docgen_opt;
  docgen_opt.include_private = options->docs_private;

  docgen_t docgen;
  docgen.errors = options->check.errors;

  doc_setup_dirs(&docgen, program, options);

  // Open the index and home files
  docgen.index_file = doc_open_file(&docgen, docgen.base_dir, "mkdocs", ".yml");
  docgen.home_file = doc_open_file(&docgen, docgen.sub_dir, "index", ".md");
  docgen.type_file = NULL;
  docgen.included_sources = NULL;

  // Write documentation files
  if(docgen.index_file != NULL && docgen.home_file != NULL)
  {
    ast_t* package = ast_child(program);
    const char* name = package_filename(package);

    // tell the mkdocs theme not index the home file for search
    fprintf(docgen.home_file, "---\n");
    fprintf(docgen.home_file, "search:\n");
    fprintf(docgen.home_file, "  exclude: true\n");
    fprintf(docgen.home_file, "---\n");

    // Print the only known content for the home file
    fprintf(docgen.home_file, "Packages\n\n");

    fprintf(docgen.index_file, "site_name: %s\n", name);
    fprintf(docgen.index_file, "theme:\n");
    fprintf(docgen.index_file, "  name: material\n");
    fprintf(docgen.index_file, "  logo: %s%s\n", PONYLANG_MKDOCS_ASSETS_DIR, PONYLANG_MKDOCS_LOGO_FILE);
    fprintf(docgen.index_file, "  favicon: %s%s\n", PONYLANG_MKDOCS_ASSETS_DIR, PONYLANG_MKDOCS_LOGO_FILE);
    fprintf(docgen.index_file, "  palette:\n");
    fprintf(docgen.index_file, "    scheme: ponylang\n");
    fprintf(docgen.index_file, "    primary: brown\n");
    fprintf(docgen.index_file, "    accent: amber\n");
    fprintf(docgen.index_file, "  features:\n");
    fprintf(docgen.index_file, "    - navigation.top\n");

    fprintf(docgen.index_file, "extra_css:\n");
    fprintf(docgen.index_file, "  - %s%s\n", PONYLANG_MKDOCS_ASSETS_DIR, PONYLANG_MKDOCS_CSS_FILE);

    fprintf(docgen.index_file, "markdown_extensions:\n");
    fprintf(docgen.index_file, "  - pymdownx.highlight:\n");
    fprintf(docgen.index_file, "      anchor_linenums: true\n");
    fprintf(docgen.index_file, "      line_anchors: \"L\"\n");
    fprintf(docgen.index_file, "      use_pygments: true\n");
    fprintf(docgen.index_file, "  - pymdownx.superfences\n");
    fprintf(docgen.index_file, "  - toc:\n");
    fprintf(docgen.index_file, "      permalink: true\n");
    fprintf(docgen.index_file, "      toc_depth: 3\n");

    fprintf(docgen.index_file, "nav:\n");
    fprintf(docgen.index_file, "- %s: index.md\n", name);

    doc_packages(&docgen, &docgen_opt, program);
  }

  if(docgen.included_sources != NULL)
  {
    fprintf(docgen.index_file, "- source:\n");
    doc_sources_t* current_source = docgen.included_sources;
    while(current_source != NULL)
    {
      fprintf(docgen.index_file, "  - %s : \"%s\" \n" , current_source->filename, current_source->doc_path);
      ponyint_pool_free_size(current_source->filename_alloc_size, (void*) current_source->filename);
      ponyint_pool_free_size(current_source->doc_path_alloc_size, (void*) current_source->doc_path);
      ponyint_pool_free_size(current_source->file_path_alloc_size, (void*) current_source->file_path);
      doc_sources_t* current_source_ptr_copy = current_source;
      current_source = current_source->next;
      ponyint_pool_free_size(sizeof(doc_sources_t), (void*) current_source_ptr_copy);
    }
  }

  // write extra css file for the theme
  FILE* css_file = doc_open_file(&docgen, docgen.assets_dir, PONYLANG_MKDOCS_CSS_FILE, "");
  if(css_file != NULL)
  {
    fprintf(css_file, PONYLANG_MKDOCS_CSS);
    fclose(css_file);
  }
  else
  {
    printf("Unable to open file %s%s", docgen.assets_dir, PONYLANG_MKDOCS_CSS_FILE);
  }

  // write logo png
  FILE* logo_file = doc_open_binary_file(&docgen, docgen.assets_dir, PONYLANG_MKDOCS_LOGO_FILE, "");
  if(logo_file != NULL)
  {
    const unsigned char logo[PONYLANG_LOGO_LEN] = PONYLANG_LOGO;
    fwrite(logo, sizeof(unsigned char), PONYLANG_LOGO_LEN, logo_file);
    fclose(logo_file);
  }
  else
  {
    printf("Unable to open file %s%s", docgen.assets_dir, PONYLANG_MKDOCS_LOGO_FILE);
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

  if(docgen.assets_dir != NULL)
    ponyint_pool_free_size(docgen.assets_dir_buf_len, (void*)docgen.assets_dir);
}
