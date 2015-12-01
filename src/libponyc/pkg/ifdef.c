#include "ifdef.h"
#include "buildflagset.h"
#include "../ast/ast.h"
#include "../ast/astbuild.h"
#include "../ast/lexer.h"
#include "../ast/symtab.h"
#include "platformfuns.h"
#include "../pass/pass.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>


// Normalise the given ifdef condition.
static void cond_normalise(ast_t** astp)
{
  assert(astp != NULL);

  ast_t* ast = *astp;
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_AND:
    {
      ast_setid(ast, TK_IFDEFAND);

      AST_GET_CHILDREN(ast, left, right);
      cond_normalise(&left);
      cond_normalise(&right);
      break;
    }

    case TK_OR:
    {
      ast_setid(ast, TK_IFDEFOR);

      AST_GET_CHILDREN(ast, left, right);
      cond_normalise(&left);
      cond_normalise(&right);
      break;
    }

    case TK_NOT:
    {
      ast_setid(ast, TK_IFDEFNOT);

      AST_GET_CHILDREN(ast, child);
      cond_normalise(&child);
      break;
    }

    case TK_STRING:
    {
      ast_setid(ast, TK_ID);

      REPLACE(astp,
        NODE(TK_IFDEFFLAG,
          TREE(*astp)));

      break;
    }

    case TK_REFERENCE:
    {
      const char* name = ast_name(ast_child(ast));
      if(strcmp(name, OS_POSIX_NAME) == 0)
      {
        REPLACE(astp,
          NODE(TK_IFDEFOR,
            NODE(TK_IFDEFOR,
              NODE(TK_IFDEFFLAG, ID(OS_LINUX_NAME))
              NODE(TK_IFDEFFLAG, ID(OS_MACOSX_NAME)))
            NODE(TK_IFDEFFLAG, ID(OS_FREEBSD_NAME))));
        break;
      }

      ast_setid(ast, TK_IFDEFFLAG);
      break;
    }

    case TK_SEQ:
    {
      // Remove the sequence node.
      assert(ast_childcount(ast) == 1);
      ast_t* child = ast_pop(ast);
      assert(child != NULL);

      cond_normalise(&child);
      ast_replace(astp, child);
      break;
    }

    case TK_IFDEFAND:
    case TK_IFDEFOR:
    case TK_IFDEFNOT:
    case TK_IFDEFFLAG:
      // Already normalised.
      break;

    default:
      ast_print(ast);
      assert(0);
      break;
  }
}


// Evaluate the given ifdef condition for the given config, or our target build
// if no config is given.
static bool cond_eval(ast_t* ast, buildflagset_t* config, bool release)
{
  assert(ast != NULL);

  switch(ast_id(ast))
  {
    case TK_NONE:
      // No condition to evaluate
      return true;

    case TK_IFDEFAND:
    {
      AST_GET_CHILDREN(ast, left, right);
      return cond_eval(left, config, release) &&
        cond_eval(right, config, release);
    }

    case TK_IFDEFOR:
    {
      AST_GET_CHILDREN(ast, left, right);
      return cond_eval(left, config, release) ||
        cond_eval(right, config, release);
    }

    case TK_IFDEFNOT:
      return !cond_eval(ast_child(ast), config, release);

    case TK_IFDEFFLAG:
    {
      const char* name = ast_name(ast_child(ast));

      if(config != NULL)
        // Lookup flag in given config.
        return buildflagset_get(config, name);

      // No config given, lookup platform flag for current build.
      bool val;
      if(os_is_target(name, release, &val))
        return val;

      // Not a platform flag, must be a user flag.
      return is_build_flag_defined(name);
    }

    default:
      ast_print(ast);
      assert(0);
      return false;
  }
}


// Find the build config flags used in the given ifdef condition.
static void find_flags_in_cond(ast_t* ast, buildflagset_t* config)
{
  assert(ast != NULL);
  assert(config != NULL);

  switch(ast_id(ast))
  {
    case TK_NONE:
      // No guard, no flags.
      break;

    case TK_IFDEFAND:
    case TK_IFDEFOR:
    {
      AST_GET_CHILDREN(ast, left, right);
      find_flags_in_cond(left, config);
      find_flags_in_cond(right, config);
      break;
    }

    case TK_IFDEFNOT:
      find_flags_in_cond(ast_child(ast), config);
      break;

    case TK_IFDEFFLAG:
    {
      const char* name = ast_name(ast_child(ast));
      buildflagset_add(config, name);
      break;
    }

    default:
      ast_print(ast);
      assert(0);
      break;
  }
}


// Find the build config flags used in the guard conditions for all FFI
// declarations for the specified FFI name in the specified package.
static bool find_decl_flags(ast_t* package, const char* ffi_name,
  buildflagset_t* config)
{
  assert(package != NULL);
  assert(ffi_name != NULL);
  assert(config != NULL);

  bool had_decl = false;

  // FFI declarations are package wide, so check all modules in package.
  for(ast_t* m = ast_child(package); m != NULL; m = ast_sibling(m))
  {
    // Find all the FFI declarations in this module.
    ast_t* use = ast_child(m);
    assert(use != NULL);

    if(ast_id(use) == TK_STRING) // Skip docstring.
      use = ast_sibling(use);

    // Check all the use commands.
    while(ast_id(use) == TK_USE)
    {
      AST_GET_CHILDREN(use, alias, decl, guard);

      if(ast_id(decl) == TK_FFIDECL && ffi_name == ast_name(ast_child(decl)))
      {
        // We have an FFI declaration for the specified name.
        had_decl = true;

        if(ast_id(guard) != TK_NONE)
          find_flags_in_cond(guard, config);
      }

      use = ast_sibling(use);
    }
  }

  return had_decl;
}


typedef struct ffi_decl_t
{
  ast_t* decl;
  const char* config;
} ffi_decl_t;


// Find the declaration for the specified FFI name that is valid for the given
// build config.
// The declaration found is stored in the given decl info argument.
// There must be exactly one valid declaration.
// If a declaration is already specified in the given decl info this must be
// the same as the one found.
// All other cases are errors, which will be reported by this function.
// Returns: true on success, false on failure.
static bool find_decl_for_config(ast_t* call, ast_t* package,
  const char* ffi_name, buildflagset_t* config, ffi_decl_t* decl_info)
{
  assert(call != NULL);
  assert(package != NULL);
  assert(ffi_name != NULL);
  assert(config != NULL);
  assert(decl_info != NULL);

  bool had_valid_decl = false;

  // FFI declarations are package wide, so check all modules in package.
  for(ast_t* m = ast_child(package); m != NULL; m = ast_sibling(m))
  {
    // Find all the FFI declarations in this module.
    ast_t* use = ast_child(m);
    assert(use != NULL);

    if(ast_id(use) == TK_STRING) // Skip docstring.
      use = ast_sibling(use);

    // Check all the use commands.
    while(ast_id(use) == TK_USE)
    {
      AST_GET_CHILDREN(use, alias, decl, guard);

      if(ast_id(decl) == TK_FFIDECL && ffi_name == ast_name(ast_child(decl)))
      {
        // We have an FFI declaration for the specified name.
        if(cond_eval(guard, config, false))
        {
          // This declaration is valid for this config.
          had_valid_decl = true;

          if(decl_info->decl != NULL)
          {
            // We already have a decalaration, is it the same one?
            if(decl_info->decl != decl)
            {
              ast_error(call, "Multiple possible declarations for FFI call");
              ast_error(decl_info->decl,
                "This declaration valid for config: %s", decl_info->config);
              ast_error(decl, "This declaration valid for config: %s",
                buildflagset_print(config));
              return false;
            }
          }
          else
          {
            // Store the declaration found.
            // We store the config string incase we need it for error messages
            // later. We stringtab it because the version we are given is in a
            // temporary buffer.
            decl_info->decl = decl;
            decl_info->config = stringtab(buildflagset_print(config));
          }
        }
      }

      use = ast_sibling(use);
    }
  }

  if(!had_valid_decl)
  {
    ast_error(call, "No FFI declaration found for %s in config: %s", ffi_name,
      buildflagset_print(config));
    return false;
  }

  return true;
}


// Check the number of configs we have to process and print a warning if it's a
// lot.
static void check_config_count(buildflagset_t* config, ast_t* location)
{
  assert(config != NULL);
  assert(location != NULL);

  double config_count = buildflagset_configcount(config);

  if(config_count > 10000)
  {
    source_t* source = ast_source(location);
    const char* file = NULL;

    if(source != NULL)
      file = source->file;

    if(file == NULL)
      file = "";

    printf("Processing %g configs at %s:"__zu", this may take some time\n",
      config_count, file, ast_line(location));
  }
}


// Find the declaration for the given FFI call that is valid for the given
// build config, within the specified ifdef condition (NULL for none).
// Returns: true on success, false on failure.
static bool find_ffi_decl(ast_t* ast, ast_t* package, ast_t* ifdef_cond,
  ast_t** out_decl)
{
  assert(ast != NULL);
  assert(package != NULL);
  assert(out_decl != NULL);

  const char* ffi_name = ast_name(ast_child(ast));
  buildflagset_t* config = buildflagset_create();

  // Find all the relevant build flags.
  if(ifdef_cond != NULL)
    find_flags_in_cond(ifdef_cond, config);

  if(!find_decl_flags(package, ffi_name, config))
  {
    // There are no declarations for this FFI.
    buildflagset_free(config);
    *out_decl = NULL;
    return true;
  }

  check_config_count(config, ast);
  ffi_decl_t decl_info = { NULL, NULL };
  buildflagset_startenum(config);

  while(buildflagset_next(config))
  {
    if(ifdef_cond == NULL || cond_eval(ifdef_cond, config, false))
    {
      // ifdef condition true, or not in an ifdef.
      // Look for valid FFI declaration.
      if(!find_decl_for_config(ast, package, ffi_name, config, &decl_info))
      {
        // Config has failed.
        buildflagset_free(config);
        return false;
      }

      assert(decl_info.decl != NULL);
    }
  }

  buildflagset_free(config);
  assert(decl_info.decl != NULL);
  *out_decl = decl_info.decl;
  return true;
}


bool ifdef_cond_normalise(ast_t** astp)
{
  assert(astp != NULL);
  assert(*astp != NULL);

  if(ast_id(*astp) == TK_NONE)  // No condition to normalise.
    return true;

  cond_normalise(astp);

  // Check whether condition can ever be true.
  buildflagset_t* config = buildflagset_create();
  find_flags_in_cond(*astp, config);
  check_config_count(config, *astp);
  buildflagset_startenum(config);

  while(buildflagset_next(config))
  {
    if(cond_eval(*astp, config, false))
      // Condition is true for this config.
      return true;
  }

  // Condition isn't true for any configs.
  return false;
}


bool ifdef_cond_eval(ast_t* ast, pass_opt_t* options)
{
  assert(ast != NULL);

  if(ast_id(ast) == TK_NONE)  // No condition to evaluate
    return true;

  return cond_eval(ast, NULL, options->release);
}


bool ffi_get_decl(typecheck_t* t, ast_t* ast, ast_t** out_decl)
{
  assert(t != NULL);
  assert(ast != NULL);
  assert(out_decl != NULL);

  const char* ffi_name = ast_name(ast_child(ast));

  // Get the symbol table for our containing ifdef (if any) directly. We can't
  // just search up through scopes as normal since FFI declarations in outer
  // scopes may not be valid within our ifdef.
  ast_t* ifdef = t->frame->ifdef_clause;
  assert(ifdef != NULL);

  symtab_t* symtab = ast_get_symtab(ifdef);
  sym_status_t status;
  ast_t* decl = symtab_find(symtab, ffi_name, &status);

  if(status == SYM_ERROR)
    // We've already found an error with that FFI name in this context.
    return false;

  if(status == SYM_NONE)
  {
    // We've not looked that up yet.
    assert(decl == NULL);
    if(!find_ffi_decl(ast, t->frame->package, t->frame->ifdef_cond, &decl))
    {
      // That went wrong. Record that so we don't try again.
      symtab_add(symtab, ffi_name, NULL, SYM_ERROR);
      return false;
    }

    // Store declaration found for next time, including if we found nothing.
    symtab_add(symtab, ffi_name, decl, SYM_FFIDECL);
  }

  *out_decl = decl;
  return true;
}
