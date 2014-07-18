#include "gentype.h"
#include "../pkg/package.h"
#include "../type/reify.h"
#include "../ds/stringtab.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

static const char* llvm_name(ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_NOMINAL:
    {
      ast_t* package = ast_child(ast);
      ast_t* name = ast_sibling(package);
      ast_t* typeargs = ast_sibling(name);
      ast_t* typearg;

      size_t len = strlen(ast_name(package)) + 1;
      len += strlen(ast_name(name)) + 1;

      typearg = ast_child(typeargs);

      while(typearg != NULL)
      {
        const char* argname = llvm_name(typearg);

        if(argname == NULL)
          return NULL;

        len += strlen(argname) + 1;
        typearg = ast_sibling(typearg);
      }

      char* fullname = malloc(len);
      fullname[0] = '\0';
      strcat(fullname, ast_name(package));
      strcat(fullname, "_");
      strcat(fullname, ast_name(name));

      typearg = ast_child(typeargs);

      while(typearg != NULL)
      {
        const char* argname = llvm_name(typearg);
        strcat(fullname, "_");
        strcat(fullname, argname);
        typearg = ast_sibling(typearg);
      }

      const char* result = stringtab(fullname);
      free(fullname);
      return result;
    }

    default:
    {
      ast_error(ast, "not implemented (name for non-nominal type)");
      break;
    }
  }

  return NULL;
}

static bool codegen_struct(compile_t* c, LLVMTypeRef type, ast_t* def,
  ast_t* typeargs)
{
  ast_t* typeparams = ast_childidx(def, 1);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member;

  member = ast_child(members);
  int count = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
        count++;
        break;

      default: {}
    }

    member = ast_sibling(member);
  }

  LLVMTypeRef* elements = malloc(sizeof(LLVMTypeRef) * count);
  member = ast_child(members);
  int index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_FVAR:
      case TK_FLET:
      {
        ast_t* ftype = ast_type(member);
        ftype = reify(ftype, typeparams, typeargs);
        LLVMTypeRef ltype = codegen_type(c, ftype);
        ast_free_unattached(ftype);

        if(ltype == NULL)
        {
          free(elements);
          return false;
        }

        elements[index] = ltype;
        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  LLVMStructSetBody(type, elements, count, false);
  LLVMDumpType(type);
  free(elements);

  return true;
}

static bool codegen_class(compile_t* c, LLVMTypeRef type, ast_t* def,
  ast_t* typeargs)
{
  if(!codegen_struct(c, type, def, typeargs))
    return false;

  return true;
}

static bool codegen_actor(compile_t* c, LLVMTypeRef type, ast_t* def,
  ast_t* typeargs)
{
  if(!codegen_struct(c, type, def, typeargs))
    return false;

  return true;
}

static LLVMTypeRef codegen_nominal(compile_t* c, ast_t* ast)
{
  assert(ast_id(ast) == TK_NOMINAL);

  // check for primitive types
  const char* name = ast_name(ast_childidx(ast, 1));

  if(!strcmp(name, "True") || !strcmp(name, "False"))
    return LLVMInt1Type();

  if(!strcmp(name, "I8") || !strcmp(name, "U8"))
    return LLVMInt8Type();

  if(!strcmp(name, "I16") || !strcmp(name, "U16"))
    return LLVMInt16Type();

  if(!strcmp(name, "I32") || !strcmp(name, "U32"))
    return LLVMInt32Type();

  if(!strcmp(name, "I64") || !strcmp(name, "U64"))
    return LLVMInt64Type();

  if(!strcmp(name, "I128") || !strcmp(name, "U128"))
    return LLVMIntType(128);

  if(!strcmp(name, "F16"))
    return LLVMHalfType();

  if(!strcmp(name, "F32"))
    return LLVMFloatType();

  if(!strcmp(name, "F64"))
    return LLVMDoubleType();

  // generate a structural type
  name = llvm_name(ast);
  LLVMTypeRef type = LLVMGetTypeByName(c->module, name);

  if(type != NULL)
    return LLVMPointerType(type, 0);

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), name);

  ast_t* def = ast_data(ast);
  ast_t* typeargs = ast_childidx(ast, 2);

  switch(ast_id(def))
  {
    case TK_TRAIT:
    {
      ast_error(ast, "not implemented (codegen for trait)");
      return NULL;
    }

    case TK_CLASS:
      if(!codegen_class(c, type, def, typeargs))
        return NULL;
      break;

    case TK_ACTOR:
      if(!codegen_actor(c, type, def, typeargs))
        return NULL;
      break;

    default:
      assert(0);
      return NULL;
  }

  return LLVMPointerType(type, 0);
}

static LLVMTypeRef codegen_union(compile_t* c, ast_t* ast)
{
  ast_error(ast, "not implemented (codegen for uniontype)");
  return NULL;
}

/**
 * TODO: structural types
 * assemble a list of all structural types in the program
 * eliminate duplicates
 * for all reified types in the program, determine if they can be each
 * structural type. if they can, add the structural type (with a vtable) to the
 * list of types that a type can be.
 */

LLVMTypeRef codegen_type(compile_t* c, ast_t* ast)
{
  switch(ast_id(ast))
  {
    case TK_UNIONTYPE:
      return codegen_union(c, ast);

    case TK_ISECTTYPE:
    {
      ast_error(ast, "not implemented (codegen for isecttype)");
      return NULL;
    }

    case TK_TUPLETYPE:
    {
      ast_error(ast, "not implemented (codegen for tupletype)");
      return NULL;
    }

    case TK_NOMINAL:
      return codegen_nominal(c, ast);

    case TK_STRUCTURAL:
    {
      ast_error(ast, "not implemented (codegen for structural)");
      return NULL;
    }

    case TK_ARROW:
      return codegen_type(c, ast_childidx(ast, 1));

    case TK_TYPEPARAMREF:
    {
      ast_error(ast, "not implemented (codegen for typeparamref)");
      return NULL;
    }

    default: {}
  }

  assert(0);
  return NULL;
}
