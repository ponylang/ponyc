#include "genfun.h"
#include "gentype.h"
#include "genname.h"
#include "gencall.h"
#include "gencontrol.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include <string.h>
#include <assert.h>

static void name_params(ast_t* params, LLVMValueRef func, bool ctor)
{
  int count = 0;

  // name the receiver 'this'
  if(!ctor)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, "this");
  }

  // name each parameter
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    LLVMValueRef fparam = LLVMGetParam(func, count++);
    LLVMSetValueName(fparam, ast_name(ast_child(param)));
    param = ast_sibling(param);
  }
}

static void start_fun(compile_t* c, LLVMValueRef fun)
{
  LLVMBasicBlockRef block = LLVMAppendBasicBlock(fun, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);
}

static ast_t* get_fun(ast_t* type, const char* name, ast_t* typeargs)
{
  // reify with both the type and the function-level typeargs
  ast_t* fun = lookup(type, name);
  assert(fun != NULL);

  if(typeargs != NULL)
  {
    ast_t* typeparams = ast_childidx(fun, 2);
    ast_t* r_fun = reify(fun, typeparams, typeargs);
    ast_free_unattached(fun);
    fun = r_fun;
    assert(fun != NULL);
  }

  return fun;
}

static LLVMValueRef get_prototype(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, ast_t* fun)
{
  // get a fully qualified name: starts with the type name, followed by the
  // type arguments, followed by the function name, followed by the function
  // level type arguments.
  const char* type_name = genname_type(type);
  const char* funname = genname_fun(type_name, name, typeargs);

  // if the function already exists, just return it
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return func;

  // count the parameters, including the receiver
  ast_t* params = ast_childidx(fun, 3);
  size_t count = ast_childcount(params) + 1;

  LLVMTypeRef tparams[count];
  count = 0;

  // get a type for the receiver
  tparams[count] = gentype(c, type);

  if(tparams[count] == NULL)
  {
    ast_error(fun, "couldn't generate receiver type");
    return NULL;
  }

  count++;

  // get a type for each parameter
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* ptype = ast_childidx(param, 1);
    tparams[count] = gentype(c, ptype);

    if(tparams[count] == NULL)
    {
      ast_error(ptype, "couldn't generate parameter type");
      return NULL;
    }

    count++;
    param = ast_sibling(param);
  }

  // get a type for the result
  ast_t* rtype = ast_childidx(fun, 4);
  LLVMTypeRef result = gentype(c, rtype);

  if(result == NULL)
  {
    ast_error(rtype, "couldn't generate result type");
    return NULL;
  }

  // generate the function type and the function prototype
  LLVMTypeRef ftype;

  // don't include the receiver for constructors
  if(ast_id(fun) == TK_NEW)
    ftype = LLVMFunctionType(result, &tparams[1], count - 1, false);
  else
    ftype = LLVMFunctionType(result, tparams, count, false);

  func = LLVMAddFunction(c->module, funname, ftype);
  name_params(params, func, ast_id(fun) == TK_NEW);

  if(ast_id(fun) != TK_FUN)
  {
    // handlers always have a receiver and have no return value
    ftype = LLVMFunctionType(LLVMVoidType(), tparams, count, false);
    const char* handler_name = genname_handler(type_name, name, typeargs);
    LLVMValueRef handler = LLVMAddFunction(c->module, handler_name, ftype);
    name_params(params, handler, false);
  }

  return func;
}

static LLVMValueRef get_handler(compile_t* c, ast_t* type, const char* name,
  ast_t* typeargs)
{
  const char* handler_name = genname_handler(genname_type(type), name,
    typeargs);
  return LLVMGetNamedFunction(c->module, handler_name);
}

static LLVMValueRef gen_newhandler(compile_t* c, ast_t* type, const char* name,
  ast_t* typeargs, ast_t* body)
{
  LLVMValueRef handler = get_handler(c, type, name, typeargs);

  if(handler == NULL)
    return NULL;

  // TODO: field initialisers
  start_fun(c, handler);
  LLVMValueRef value = gen_seq(c, body);

  if(value == NULL)
    return NULL;

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c, handler);
  return handler;
}

static void set_descriptor(compile_t* c, ast_t* type, LLVMValueRef this_ptr)
{
  LLVMSetValueName(this_ptr, "this");

  LLVMValueRef desc = LLVMGetNamedGlobal(c->module,
    genname_descriptor(genname_type(type)));
  desc = LLVMBuildBitCast(c->builder, desc, c->descriptor_ptr, "");

  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 0, "");
  LLVMBuildStore(c->builder, desc, desc_ptr);
}

LLVMValueRef genfun(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(type, name, typeargs);
  LLVMValueRef func = get_prototype(c, type, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  start_fun(c, func);
  LLVMValueRef value = gen_seq(c, ast_childidx(fun, 6));

  if(value == NULL)
    return NULL;

  LLVMBuildRet(c->builder, value);
  codegen_finishfun(c, func);
  return func;
}

LLVMValueRef genfun_be(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, int index)
{
  ast_t* fun = get_fun(type, name, typeargs);
  LLVMValueRef func = get_prototype(c, type, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  // TODO: send a message to 'this'
  start_fun(c, func);
  LLVMValueRef this_ptr = LLVMGetParam(func, 0);

  // return 'this'
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c, func);

  LLVMValueRef handler = get_handler(c, type, name, typeargs);

  if(handler == NULL)
    return NULL;

  start_fun(c, handler);
  LLVMValueRef value = gen_seq(c, ast_childidx(fun, 6));

  if(value == NULL)
    return NULL;

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c, handler);
  return func;
}

LLVMValueRef genfun_new(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(type, name, typeargs);
  LLVMValueRef func = get_prototype(c, type, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  // allocate the object as 'this'
  start_fun(c, func);
  LLVMTypeRef p_type = gentype(c, type);

  if(p_type == NULL)
    return NULL;

  LLVMValueRef this_ptr = gencall_alloc(c, p_type);
  set_descriptor(c, type, this_ptr);

  // call the handler
  LLVMValueRef handler = get_handler(c, type, name, typeargs);

  if(handler == NULL)
    return NULL;

  int count = LLVMCountParamTypes(LLVMGetElementType(LLVMTypeOf(handler)));
  LLVMValueRef args[count];
  args[0] = this_ptr;

  for(int i = 1; i < count; i++)
    args[i] = LLVMGetParam(func, i - 1);

  LLVMBuildCall(c->builder, handler, args, count, "");

  // return 'this'
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c, func);

  // generate the handler
  handler = gen_newhandler(c, type, name, typeargs, ast_childidx(fun, 6));

  if(handler == NULL)
    return NULL;

  return func;
}

LLVMValueRef genfun_newbe(compile_t* c, ast_t* type, const char *name,
  ast_t* typeargs, int index)
{
  ast_t* fun = get_fun(type, name, typeargs);
  LLVMValueRef func = get_prototype(c, type, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  // allocate the actor as 'this'
  start_fun(c, func);
  LLVMTypeRef p_type = gentype(c, type);

  if(p_type == NULL)
    return NULL;

  // TODO: don't heap alloc!
  LLVMValueRef this_ptr = gencall_alloc(c, p_type);
  set_descriptor(c, type, this_ptr);

  // TODO: initialise the actor
  // TODO: send a message to 'this'

  // return 'this'
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c, func);

  LLVMValueRef handler = gen_newhandler(c, type, name, typeargs,
    ast_childidx(fun, 6));

  if(handler == NULL)
    return NULL;

  return func;
}
