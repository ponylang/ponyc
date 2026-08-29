#include "genexport.h"
#include "../ast/ast.h"
#include "../ast/id.h"
#include "../ast/printbuf.h"
#include "../ast/stringtab.h"
#include "../pkg/package.h"
#include "paths.h"
#include "ponyassert.h"
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_IS_WINDOWS
# define PATH_SLASH '\\'
#else
# define PATH_SLASH '/'
#endif

static const char* c_type_for_pony_name(const char* name)
{
  if(strcmp(name, "Bool") == 0) return "bool";
  if(strcmp(name, "I8") == 0) return "int8_t";
  if(strcmp(name, "I16") == 0) return "int16_t";
  if(strcmp(name, "I32") == 0) return "int32_t";
  if(strcmp(name, "I64") == 0) return "int64_t";
  if(strcmp(name, "I128") == 0) return "__int128_t";
  if(strcmp(name, "ILong") == 0) return "long";
  if(strcmp(name, "ISize") == 0) return "intptr_t";
  if(strcmp(name, "U8") == 0) return "uint8_t";
  if(strcmp(name, "U16") == 0) return "uint16_t";
  if(strcmp(name, "U32") == 0) return "uint32_t";
  if(strcmp(name, "U64") == 0) return "uint64_t";
  if(strcmp(name, "U128") == 0) return "__uint128_t";
  if(strcmp(name, "ULong") == 0) return "unsigned long";
  if(strcmp(name, "USize") == 0) return "size_t";
  if(strcmp(name, "F32") == 0) return "float";
  if(strcmp(name, "F64") == 0) return "double";
  if(strcmp(name, "None") == 0) return "void";
  return NULL;
}

static ast_t* resolve_typearg(ast_t* type, ast_t* class_typeparams,
  ast_t* typeargs)
{
  if(type == NULL || class_typeparams == NULL || typeargs == NULL)
    return type;

  if(ast_id(type) != TK_TYPEPARAMREF)
    return type;

  ast_t* ref_def = (ast_t*)ast_data(type);

  if(ref_def == NULL)
    return type;

  size_t index = 0;

  for(ast_t* tp = ast_child(class_typeparams); tp != NULL;
    tp = ast_sibling(tp))
  {
    if(tp == ref_def)
    {
      ast_t* arg = ast_childidx(typeargs, index);

      if(arg != NULL)
        return arg;

      break;
    }

    index++;
  }

  return type;
}

static void print_c_type(printbuf_t* buf, ast_t* type,
  ast_t* class_typeparams, ast_t* typeargs)
{
  if(type == NULL || ast_id(type) == TK_NONE)
  {
    printbuf(buf, "void");
    return;
  }

  type = resolve_typearg(type, class_typeparams, typeargs);

  if(ast_id(type) == TK_NOMINAL)
  {
    ast_t* type_id = ast_childidx(type, 1);
    const char* name = ast_name(type_id);
    const char* c_name = c_type_for_pony_name(name);

    if(c_name != NULL)
      printbuf(buf, "%s", c_name);
    else
      printbuf(buf, "void*");
  }
  else
  {
    printbuf(buf, "void*");
  }
}

bool package_has_exports(ast_t* package, pass_opt_t* opt)
{
  for(ast_t* module = ast_child(package); module != NULL;
    module = ast_sibling(module))
  {
    for(ast_t* node = ast_child(module); node != NULL;
      node = ast_sibling(node))
    {
      token_id id = ast_id(node);

      if(id != TK_CLASS && id != TK_PRIMITIVE && id != TK_STRUCT &&
        id != TK_ACTOR && id != TK_TYPE)
        continue;

      if(ast_has_annotation(node, "c_api", opt->strtab))
        return true;
    }
  }

  return false;
}

bool resolve_export_alias(ast_t* node, token_id nid, ast_t** def,
  token_id* def_nid, ast_t** typeargs, ast_t** class_typeparams)
{
  *def = node;
  *def_nid = nid;
  *typeargs = NULL;
  *class_typeparams = NULL;

  if(nid == TK_TYPE)
  {
    ast_t* provides = ast_childidx(node, 3);

    if(ast_id(provides) == TK_NONE)
      return false;

    ast_t* nominal = ast_child(provides);

    if(nominal == NULL || ast_id(nominal) != TK_NOMINAL)
      return false;

    *def = (ast_t*)ast_data(nominal);

    if(*def == NULL)
      return false;

    *def_nid = ast_id(*def);
    *typeargs = ast_childidx(nominal, 2);

    if(ast_id(*typeargs) == TK_NONE)
      *typeargs = NULL;

    *class_typeparams = ast_childidx(*def, 1);

    if(ast_id(*class_typeparams) == TK_NONE)
      *class_typeparams = NULL;
  }

  return true;
}

static bool export_has_tuple_types(ast_t* method, ast_t* class_typeparams,
  ast_t* typeargs)
{
  ast_t* return_type = ast_childidx(method, 4);

  if(return_type != NULL && ast_id(return_type) != TK_NONE)
  {
    ast_t* resolved = resolve_typearg(return_type, class_typeparams, typeargs);

    if(ast_id(resolved) == TK_TUPLETYPE)
      return true;
  }

  ast_t* params = ast_childidx(method, 3);

  if(ast_id(params) != TK_NONE)
  {
    for(ast_t* param = ast_child(params); param != NULL;
      param = ast_sibling(param))
    {
      ast_t* param_type = ast_childidx(param, 1);
      ast_t* resolved = resolve_typearg(param_type, class_typeparams, typeargs);

      if(ast_id(resolved) == TK_TUPLETYPE)
        return true;
    }
  }

  return false;
}

bool should_export_method(ast_t* method, ast_t* class_typeparams,
  ast_t* typeargs)
{
  ast_t* cap = ast_child(method);
  ast_t* method_id = ast_childidx(method, 1);
  ast_t* method_typeparams = ast_childidx(method, 2);
  ast_t* question = ast_childidx(method, 5);
  const char* method_name = ast_name(method_id);

  if(ast_id(cap) == TK_AT)
    return false;

  if(method_name[0] == '_')
    return false;

  if(ast_id(method_typeparams) != TK_NONE)
    return false;

  if(ast_id(question) == TK_QUESTION)
    return false;

  if(export_has_tuple_types(method, class_typeparams, typeargs))
    return false;

  return true;
}

static void print_method_signature(printbuf_t* buf, const char* type_name,
  ast_t* method, ast_t* class_typeparams, ast_t* typeargs, bool is_primitive)
{
  if(!should_export_method(method, class_typeparams, typeargs))
    return;

  ast_t* method_id = ast_childidx(method, 1);
  ast_t* params = ast_childidx(method, 3);
  ast_t* return_type = ast_childidx(method, 4);
  const char* method_name = ast_name(method_id);
  bool first_param = true;

  char sanitized_method[256];
  strncpy(sanitized_method, method_name, sizeof(sanitized_method) - 1);
  sanitized_method[sizeof(sanitized_method) - 1] = '\0';
  sanitize_c_name(sanitized_method);

  printbuf(buf, "extern ");
  print_c_type(buf, return_type, class_typeparams, typeargs);
  printbuf(buf, " %s_%s(", type_name, sanitized_method);

  if(!is_primitive)
  {
    printbuf(buf, "void* pony_this");
    first_param = false;
  }

  if(ast_id(params) != TK_NONE)
  {
    for(ast_t* param = ast_child(params); param != NULL;
      param = ast_sibling(param))
    {
      ast_t* param_id = ast_child(param);
      ast_t* param_type = ast_childidx(param, 1);

      if(!first_param)
        printbuf(buf, ", ");

      char sanitized_param[256];
      strncpy(sanitized_param, ast_name(param_id),
        sizeof(sanitized_param) - 1);
      sanitized_param[sizeof(sanitized_param) - 1] = '\0';
      sanitize_c_name(sanitized_param);

      print_c_type(buf, param_type, class_typeparams, typeargs);
      printbuf(buf, " %s", sanitized_param);
      first_param = false;
    }
  }

  if(first_param)
    printbuf(buf, "void");

  printbuf(buf, ");\n");
}

void sanitize_c_name(char* name)
{
  for(char* p = name; *p != '\0'; p++)
  {
    if(!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
      (*p >= '0' && *p <= '9') || *p == '_'))
      *p = '_';
  }
}

static bool gen_consumer_header(ast_t* exported_pkg, const char* header_name,
  const char* sym_prefix, const char* output, pass_opt_t* opt)
{
  char header_path[FILENAME_MAX];
  int written = snprintf(header_path, sizeof(header_path), "%s%c%s_export.h",
    output, PATH_SLASH, header_name);

  if((written < 0) || ((size_t)written >= sizeof(header_path)))
  {
    errorf(opt->check.errors, NULL, "export header path is too long");
    return false;
  }

  FILE* fp = fopen(header_path, "wt");

  if(fp == NULL)
  {
    errorf(opt->check.errors, NULL, "couldn't write export header to %s",
      header_path);
    return false;
  }

  fprintf(fp,
    "#pragma once\n"
    "\n"
    "/* Auto-generated by ponyc -- do not edit. */\n"
    "\n"
    "#include <stdint.h>\n"
    "#include <stdbool.h>\n"
    "\n"
    "#if defined(_MSC_VER) && !defined(__clang__)\n"
    "typedef struct __int128_t { uint64_t low; int64_t high; } __int128_t;\n"
    "typedef struct __uint128_t { uint64_t low; uint64_t high; } "
      "__uint128_t;\n"
    "#endif\n"
    "\n"
    "#ifdef __cplusplus\n"
    "extern \"C\" {\n"
    "#endif\n"
    "\n");

  printbuf_t* buf = printbuf_new();

  for(ast_t* module = ast_child(exported_pkg); module != NULL;
    module = ast_sibling(module))
  {
    for(ast_t* node = ast_child(module); node != NULL;
      node = ast_sibling(node))
    {
      token_id nid = ast_id(node);

      if(nid != TK_CLASS && nid != TK_PRIMITIVE && nid != TK_STRUCT &&
        nid != TK_ACTOR && nid != TK_TYPE)
        continue;

      if(!ast_has_annotation(node, "c_api", opt->strtab))
        continue;

      const char* bare_name = ast_name(ast_child(node));

      ast_t* def;
      token_id def_nid;
      ast_t* typeargs;
      ast_t* class_typeparams;

      if(!resolve_export_alias(node, nid, &def, &def_nid, &typeargs,
        &class_typeparams))
        continue;

      if(def_nid == TK_TRAIT || def_nid == TK_INTERFACE)
      {
        ast_error(opt->check.errors, node,
          "type alias for \\c_api\\ must refer to a class, primitive, "
          "struct, or actor, not %s",
          def_nid == TK_TRAIT ? "a trait" : "an interface");
        printbuf_free(buf);
        fclose(fp);
        remove(header_path);
        return false;
      }

      printbuf_t* namebuf = printbuf_new();

      if(sym_prefix[0] != '\0')
        printbuf(namebuf, "%s_%s", sym_prefix, bare_name);
      else
        printbuf(namebuf, "%s", bare_name);

      sanitize_c_name(namebuf->m);
      const char* type_name = namebuf->m;

      printbuf(buf, "/* %s */\n", type_name);

      ast_t* members = ast_childidx(def, 4);
      int exported_count = 0;

      if(ast_id(members) != TK_NONE)
      {
        for(ast_t* member = ast_child(members); member != NULL;
          member = ast_sibling(member))
        {
          if(ast_id(member) == TK_FUN &&
            should_export_method(member, class_typeparams, typeargs))
          {
            print_method_signature(buf, type_name, member,
              class_typeparams, typeargs, def_nid == TK_PRIMITIVE);
            exported_count++;
          }
        }
      }

      if(exported_count == 0)
      {
        ast_error(opt->check.errors, node,
          "exported type '%s' has no exportable methods", bare_name);
        printbuf_free(namebuf);
        printbuf_free(buf);
        fclose(fp);
        remove(header_path);
        return false;
      }

      printbuf_free(namebuf);
      printbuf(buf, "\n");
    }
  }

  fwrite(buf->m, 1, buf->offset, fp);
  printbuf_free(buf);

  fprintf(fp,
    "#ifdef __cplusplus\n"
    "}\n"
    "#endif\n");

  fclose(fp);

  if(opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Export header: %s\n", header_path);

  return true;
}

const char* resolve_use_name(ast_t* use_node)
{
  ast_t* alias_id = ast_child(use_node);
  ast_t* locator = ast_childidx(use_node, 1);

  if(ast_id(locator) != TK_STRING)
    return NULL;

  if(ast_id(alias_id) == TK_ID)
    return ast_name(alias_id);

  const char* path = ast_name(locator);
  const char* slash = strrchr(path, '/');
  return (slash != NULL) ? slash + 1 : path;
}

bool genexport_header(ast_t* program, pass_opt_t* opt)
{
  bool ok = true;
  const char* output = (opt->output != NULL) ? opt->output : ".";

  pony_mkdir(output);

  for(ast_t* package = ast_child(program); package != NULL;
    package = ast_sibling(package))
  {
    for(ast_t* module = ast_child(package); module != NULL;
      module = ast_sibling(module))
    {
      for(ast_t* node = ast_child(module); node != NULL;
        node = ast_sibling(node))
      {
        if(ast_id(node) != TK_USE)
          continue;

        const char* use_name = resolve_use_name(node);

        if(use_name == NULL)
          continue;

        ast_t* used_pkg = (ast_t*)ast_data(node);

        if(used_pkg == NULL || ast_id(used_pkg) != TK_PACKAGE)
          continue;

        if(!package_has_exports(used_pkg, opt))
          continue;

        package_set_needs_export_include(package);

        printbuf_t* prefix_buf = printbuf_new();
        printbuf(prefix_buf, "%s", use_name);
        sanitize_c_name(prefix_buf->m);

        if(!gen_consumer_header(used_pkg, use_name, prefix_buf->m, output, opt))
          ok = false;

        printbuf_free(prefix_buf);
      }
    }
  }

  ast_t* main_pkg = ast_child(program);

  if(package_has_exports(main_pkg, opt))
  {
    const char* main_name = package_filename(main_pkg);

    if((opt->bin_name != NULL) && (opt->bin_name[0] != '\0'))
      main_name = opt->bin_name;

    package_set_needs_export_include(main_pkg);

    if(!gen_consumer_header(main_pkg, main_name, "", output, opt))
      ok = false;
  }

  return ok;
}
