#include "genfun.h"
#include "gentype.h"
#include "genname.h"
#include "gencall.h"
#include "gencontrol.h"
#include "../type/reify.h"
#include "../type/lookup.h"
#include "../ds/hash.h"
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

static ast_t* get_fun(gentype_t* g, const char* name, ast_t* typeargs)
{
  // reify with both the type and the function-level typeargs
  ast_t* fun = lookup(g->ast, name);
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

static LLVMValueRef get_prototype(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, ast_t* fun)
{
  // Get a fully qualified name: starts with the type name, followed by the
  // type arguments, followed by the function name, followed by the function
  // level type arguments.
  const char* funname = genname_fun(g->type_name, name, typeargs);

  // If the function already exists, just return it.
  LLVMValueRef func = LLVMGetNamedFunction(c->module, funname);

  if(func != NULL)
    return func;

  // count the parameters, including the receiver
  ast_t* params = ast_childidx(fun, 3);
  size_t count = ast_childcount(params) + 1;

  LLVMTypeRef tparams[count];
  count = 0;

  // get a type for the receiver
  tparams[count++] = g->use_type;

  // get a type for each parameter
  ast_t* param = ast_child(params);

  while(param != NULL)
  {
    ast_t* ptype = ast_childidx(param, 1);
    gentype_t ptype_g;

    if(!gentype(c, ptype, &ptype_g))
    {
      ast_error(ptype, "couldn't generate parameter type");
      return NULL;
    }

    tparams[count++] = ptype_g.use_type;
    param = ast_sibling(param);
  }

  // get a type for the result
  ast_t* rtype = ast_childidx(fun, 4);
  gentype_t rtype_g;

  if(!gentype(c, rtype, &rtype_g))
  {
    ast_error(rtype, "couldn't generate result type");
    return NULL;
  }

  LLVMTypeRef result = rtype_g.use_type;

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
    const char* handler_name = genname_handler(g->type_name, name, typeargs);
    LLVMValueRef handler = LLVMAddFunction(c->module, handler_name, ftype);
    name_params(params, handler, false);
  }

  return func;
}

static LLVMValueRef get_handler(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs)
{
  const char* handler_name = genname_handler(g->type_name, name, typeargs);
  return LLVMGetNamedFunction(c->module, handler_name);
}

static LLVMValueRef gen_newhandler(compile_t* c, gentype_t* g, const char* name,
  ast_t* typeargs, ast_t* body)
{
  LLVMValueRef handler = get_handler(c, g, name, typeargs);

  if(handler == NULL)
    return NULL;

  // TODO: field initialisers
  codegen_startfun(c, handler);
  LLVMValueRef value = gen_seq(c, body);

  if(value == NULL)
    return NULL;

  LLVMBuildRetVoid(c->builder);
  codegen_finishfun(c);
  return handler;
}

static void set_descriptor(compile_t* c, gentype_t* g, LLVMValueRef this_ptr)
{
  LLVMSetValueName(this_ptr, "this");
  LLVMValueRef desc_ptr = LLVMBuildStructGEP(c->builder, this_ptr, 0, "");
  LLVMBuildStore(c->builder, g->desc, desc_ptr);
}

LLVMValueRef genfun_proto(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  return get_prototype(c, g, name, typeargs, fun);
}

LLVMValueRef genfun_fun(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  codegen_startfun(c, func);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_seq(c, body);

  if(value == NULL)
  {
    return NULL;
  } else if(value != GEN_NOVALUE) {
    LLVMBuildRet(c->builder, value);
  }

  codegen_finishfun(c);
  return func;
}

LLVMValueRef genfun_be(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, int index)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  codegen_startfun(c, func);

  LLVMValueRef this_ptr = LLVMGetParam(func, 0);

  // Get the parameter types. Leave room for one more at the beginning.
  LLVMTypeRef f_type = LLVMTypeOf(func);
  size_t count = LLVMCountParamTypes(f_type) + 1;
  LLVMTypeRef params[count];
  LLVMGetParamTypes(f_type, &params[1]);

  // The first one becomes the message ID. Replace the this pointer with the
  // message index, then create a type for this message.
  params[0] = LLVMInt32Type();
  params[1] = LLVMInt32Type();
  LLVMTypeRef msg_type = LLVMStructType(params, count, false);

  // Calculate the index (power of 2) for the message size.
  size_t size = LLVMABISizeOfType(c->target, msg_type);
  size = next_pow2(size);

  // Subtract 7 because we are looking to make 64 come out to zero.
  if(size <= 64)
    size = 0;
  else
    size = __builtin_ffsl(size) - 7;

  // Allocate the message, setting its ID and index.
  LLVMValueRef args[2];
  args[0] = LLVMConstInt(LLVMInt32Type(), size, false);
  args[1] = LLVMConstInt(LLVMInt32Type(), index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = LLVMBuildBitCast(c->builder, msg,
    LLVMPointerType(msg_type, 0), "");

  // Populate the message contents.
  for(int i = 1; i < (count - 1); i++)
  {
    LLVMValueRef arg = LLVMGetParam(func, i);
    LLVMValueRef arg_ptr = LLVMBuildStructGEP(c->builder, msg_ptr, i + 1, "");
    LLVMBuildStore(c->builder, arg, arg_ptr);
  }

  // Send the message.
  args[0] = this_ptr;
  args[1] = msg;
  gencall_runtime(c, "pony_sendv", args, 2, "");

  // return 'this'
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  LLVMValueRef handler = get_handler(c, g, name, typeargs);

  if(handler == NULL)
    return NULL;

  codegen_startfun(c, handler);

  ast_t* body = ast_childidx(fun, 6);
  LLVMValueRef value = gen_seq(c, body);

  if(value == NULL)
  {
    return NULL;
  } else if(value != GEN_NOVALUE) {
    LLVMBuildRetVoid(c->builder);
  }

  codegen_finishfun(c);
  return func;
}

LLVMValueRef genfun_new(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  codegen_startfun(c, func);

  // allocate the object as 'this'
  LLVMValueRef this_ptr = gencall_alloc(c, g->use_type);
  set_descriptor(c, g, this_ptr);

  // call the handler
  LLVMValueRef handler = get_handler(c, g, name, typeargs);

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
  codegen_finishfun(c);

  // generate the handler
  handler = gen_newhandler(c, g, name, typeargs, ast_childidx(fun, 6));

  if(handler == NULL)
    return NULL;

  return func;
}

LLVMValueRef genfun_newbe(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs, int index)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  codegen_startfun(c, func);

  // allocate the actor as 'this'
  // TODO: don't heap alloc!
  LLVMValueRef this_ptr = gencall_alloc(c, g->use_type);
  set_descriptor(c, g, this_ptr);

  // TODO: initialise the actor
  // TODO: send a message to 'this'

  // return 'this'
  LLVMBuildRet(c->builder, this_ptr);
  codegen_finishfun(c);

  LLVMValueRef handler = gen_newhandler(c, g, name, typeargs,
    ast_childidx(fun, 6));

  if(handler == NULL)
    return NULL;

  return func;
}

LLVMValueRef genfun_newdata(compile_t* c, gentype_t* g, const char *name,
  ast_t* typeargs)
{
  ast_t* fun = get_fun(g, name, typeargs);
  LLVMValueRef func = get_prototype(c, g, name, typeargs, fun);

  if(func == NULL)
    return NULL;

  // Return the constant global instance.
  codegen_startfun(c, func);
  LLVMBuildRet(c->builder, g->instance);
  codegen_finishfun(c);

  return func;
}

bool genfun_methods(compile_t* c, gentype_t* g)
{
  ast_t* def = ast_data(g->ast);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);
  int be_index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      {
        AST_GET_CHILDREN(member, ignore, id, typeparams);

        if(ast_id(typeparams) != TK_NONE)
        {
          // TODO: polymorphic constructors
          ast_error(typeparams,
            "not implemented (codegen for polymorphic constructors)");
          return false;
        }

        LLVMValueRef fun;

        switch(g->underlying)
        {
          case TK_ACTOR:
            fun = genfun_newbe(c, g, ast_name(id), NULL, be_index++);
            break;

          case TK_DATA:
            fun = genfun_newdata(c, g, ast_name(id), NULL);
            break;

          default:
            fun = genfun_new(c, g, ast_name(id), NULL);
        }

        if(fun == NULL)
          return false;

        break;
      }

      case TK_BE:
      {
        AST_GET_CHILDREN(member, ignore, id, typeparams);

        if(ast_id(typeparams) != TK_NONE)
        {
          // TODO: polymorphic behaviours
          ast_error(typeparams,
            "not implemented (codegen for polymorphic behaviours)");
          return false;
        }

        LLVMValueRef fun = genfun_be(c, g, ast_name(id), NULL, be_index++);

        if(fun == NULL)
          return false;

        break;
      }

      case TK_FUN:
      {
        AST_GET_CHILDREN(member, ignore, id, typeparams);

        if(ast_id(typeparams) != TK_NONE)
        {
          // TODO: polymorphic functions
          ast_error(typeparams,
            "not implemented (codegen for polymorphic functions)");
          return false;
        }

        LLVMValueRef fun = genfun_fun(c, g, ast_name(id), NULL);

        if(fun == NULL)
          return false;

        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return true;
}
