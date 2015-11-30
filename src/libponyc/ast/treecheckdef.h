#ifndef TREE_CHECK
#error "This file should only be included by treecheck.c"
#endif

// TODO: big comment explaining what's going on
// TODO: ast type field

ROOT(program)

RULE(program,
  IS_SCOPE  // path -> PACKAGE
  HAS_DATA  // program_t
  ZERO_OR_MORE(package),
  TK_PROGRAM);

RULE(package,
  IS_SCOPE  // name -> entity
  HAS_DATA  // package_t
  ZERO_OR_MORE(module)
  OPTIONAL(string),
  TK_PACKAGE);

RULE(module,
  IS_SCOPE  // name -> PACKAGE | entity
  HAS_DATA  // source_t
  OPTIONAL(string)
  ZERO_OR_MORE(use)
  ZERO_OR_MORE(class_def),
  TK_MODULE);

RULE(use,
  HAS_DATA  // Included package (unaliased use package commands only)
  CHILD(id, none)
  CHILD(ffidecl, string)
  CHILD(expr, ifdef_cond, none),  // Guard
  TK_USE);

RULE(ffidecl,
  IS_SCOPE
  CHILD(id, string)
  CHILD(type_args)  // Return type
  CHILD(params, none)
  CHILD(none) // Named params
  CHILD(question, none),
  TK_FFIDECL);

RULE(class_def,
  IS_SCOPE  // name -> TYPEPARAM | FVAR | FVAL | EMBED | method
  //HAS_DATA  // Type checking state
  CHILD(id)
  CHILD(type_params, none)
  CHILD(cap, none)
  CHILD(provides, none)
  CHILD(members, none)
  CHILD(at, none)
  CHILD(string, none), // Doc
  TK_TYPE, TK_INTERFACE, TK_TRAIT, TK_PRIMITIVE, TK_STRUCT, TK_CLASS,
  TK_ACTOR);

RULE(provides, ONE_OR_MORE(type), TK_PROVIDES);

RULE(members,
  ZERO_OR_MORE(field)
  ZERO_OR_MORE(method),
  TK_MEMBERS);

RULE(field,
  CHILD(id)
  CHILD(type, none)  // Field type
  CHILD(expr, none)
  CHILD(provides, none), // Delegate
  TK_FLET, TK_FVAR, TK_EMBED);

RULE(method,
  IS_SCOPE  // name -> TYPEPARAM | PARAM
  HAS_DATA  // Body donor type
  CHILD(cap, none)
  CHILD(id)
  CHILD(type_params, none)
  CHILD(params, none)
  CHILD(type, none) // Return type
  CHILD(question, none)
  CHILD(seq, none)  // Body
  CHILD(string, none)
  CHILD(seq, none), // Guard (case methods only)
  TK_FUN, TK_NEW, TK_BE);

RULE(type_params, ONE_OR_MORE(type_param), TK_TYPEPARAMS);

RULE(type_param,
  CHILD(id)
  CHILD(type, none)   // Constraint
  CHILD(type, none),  // Default
  TK_TYPEPARAM);

RULE(type_args, ONE_OR_MORE(type), TK_TYPEARGS);

RULE(params,
  ONE_OR_MORE(param)
  OPTIONAL(ellipsis),
  TK_PARAMS);

RULE(param,
  CHILD(id, expr)
  CHILD(type, none)
  CHILD(expr, none),
  TK_PARAM);

RULE(seq, MAYBE_SCOPE ONE_OR_MORE(jump, expr, semi), TK_SEQ);

RULE(jump,
  CHILD(seq, none),
  TK_RETURN, TK_BREAK, TK_CONTINUE, TK_ERROR, TK_COMPILE_INTRINSIC,
  TK_COMPILE_ERROR);

GROUP(expr,
  local, infix, asop, tuple, consume, recover, prefix, dot, tilde,
  qualify, call, ffi_call,
  if_expr, ifdef, loop, for_loop, with, match, try_expr, lambda,
  array_literal, object_literal, int_literal, float_literal, string,
  bool_literal, id, seq,
  this_ref, ref, fun_ref, type_ref, flet_ref, field_ref, local_ref);

RULE(local,
  CHILD(id)
  CHILD(type, none),
  TK_LET, TK_VAR);

RULE(infix,
  // RHS first for TK_ASSIGN, to handle init tracking
  // LHS first for all others
  CHILD(expr)
  CHILD(expr),
  TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD, TK_LSHIFT, TK_RSHIFT,
  TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE, TK_IS, TK_ISNT,
  TK_AND, TK_OR, TK_XOR, TK_ASSIGN);

RULE(asop,
  CHILD(expr)
  CHILD(type),
  TK_AS);

RULE(tuple, ONE_OR_MORE(seq, dont_care), TK_TUPLE);

RULE(consume,
  CHILD(cap, borrowed, none)
  CHILD(expr),
  TK_CONSUME);

RULE(recover,
  IS_SCOPE
  CHILD(cap, none)
  CHILD(expr),
  TK_RECOVER);

RULE(prefix,
  CHILD(expr),
  TK_NOT, TK_UNARY_MINUS, TK_ADDRESS, TK_IDENTITY);

RULE(dot,
  CHILD(expr)
  CHILD(id, int_literal, type_args),
  TK_DOT);

RULE(tilde,
  CHILD(expr)
  CHILD(id),
  TK_TILDE);

RULE(qualify,
  CHILD(expr)
  CHILD(type_args),
  TK_QUALIFY);

RULE(call,
  CHILD(positional_args, none)
  CHILD(named_args, none)
  CHILD(expr),  // Note that receiver comes last
  TK_CALL);

RULE(ffi_call,
  CHILD(id, string)
  CHILD(type_args, none)
  CHILD(positional_args, none)
  CHILD(named_args, none)
  CHILD(question, none),
  TK_FFICALL);

RULE(positional_args, ONE_OR_MORE(seq), TK_POSITIONALARGS);

RULE(named_args, ONE_OR_MORE(named_arg), TK_NAMEDARGS);

RULE(named_arg,
  CHILD(id)
  CHILD(seq),
  TK_NAMEDARG, TK_UPDATEARG);

RULE(ifdef,
  IS_SCOPE
  CHILD(expr, ifdef_cond)   // Then expression
  CHILD(seq)                // Then body
  CHILD(seq, ifdef, none)   // Else body
  CHILD(none, ifdef_cond),  // Else expression
  TK_IFDEF);

GROUP(ifdef_cond,
  ifdef_infix, ifdef_not, ifdef_flag);

RULE(ifdef_infix,
  CHILD(ifdef_cond)
  CHILD(ifdef_cond),
  TK_IFDEFAND, TK_IFDEFOR);

RULE(ifdef_not,
  CHILD(ifdef_cond),
  TK_IFDEFNOT);

RULE(ifdef_flag,
  CHILD(id),
  TK_IFDEFFLAG);

RULE(if_expr,
  IS_SCOPE
  CHILD(seq)  // Condition (has no symbol table)
  CHILD(seq)  // Then body
  CHILD(seq, if_expr, none), // Else body
  TK_IF);

RULE(loop,
  IS_SCOPE
  CHILD(seq)        // Condition
  CHILD(seq)        // Loop body
  CHILD(seq, none), // Else body
  TK_WHILE, TK_REPEAT);
  // TODO: This is wrong for repeat, check parser.c is actually how we want it

RULE(for_loop,
  CHILD(expr) // Iterator declaration
  CHILD(seq)  // Iterator value
  CHILD(seq)  // Loop body
  CHILD(seq, none), // Else body
  TK_FOR);

RULE(with,
  CHILD(expr) // With variable(s)
  CHILD(seq)  // Body
  CHILD(seq, none), // Else
  TK_WITH);

RULE(match,
  IS_SCOPE
  CHILD(expr)
  CHILD(cases, none)
  CHILD(seq, none),  // Else body
  TK_MATCH);

RULE(cases, IS_SCOPE ZERO_OR_MORE(match_case), TK_CASES);

RULE(match_case,
  IS_SCOPE
  CHILD(expr, none)
  CHILD(seq, none)   // Guard
  CHILD(seq, none),  // Body
  TK_CASE);

RULE(try_expr,
  // the then_clause holds the LLVMValueRef for the indirectbr instruction
  CHILD(seq)  // Try body
  CHILD(seq, none)  // Else body
  CHILD(seq, none), // Then body
  TK_TRY, TK_TRY_NO_CHECK);

RULE(lambda,
  CHILD(cap, none)
  CHILD(type_params, none)
  CHILD(params, none)
  CHILD(lambda_captures, none)
  CHILD(type, none) // Return
  CHILD(question, none)
  CHILD(seq),
  TK_LAMBDA);

RULE(lambda_captures, ONE_OR_MORE(lambda_capture), TK_LAMBDACAPTURES);

RULE(lambda_capture,
  CHILD(id)
  CHILD(type, none)
  CHILD(expr, none),
  TK_LAMBDACAPTURE);

RULE(array_literal,
  CHILD(type, none)
  ONE_OR_MORE(expr),
  TK_ARRAY);

RULE(object_literal,
  CHILD(cap, none)
  CHILD(provides, none)
  CHILD(members),
  TK_OBJECT);

RULE(ref,
  CHILD(id),
  TK_REFERENCE, TK_PACKAGEREF, TK_PARAMREF);

RULE(fun_ref,
  CHILD(expr)
  CHILD(id, type_args),
  TK_FUNREF, TK_BEREF, TK_NEWREF, TK_NEWBEREF);

RULE(type_ref,
  CHILD(expr)
  OPTIONAL(id, type_args),
  TK_TYPEREF);

RULE(field_ref,
  CHILD(expr)
  CHILD(id),
  TK_FVARREF, TK_EMBEDREF);

RULE(flet_ref,
  CHILD(expr)
  CHILD(id, int_literal), // Int for tuple element access
  TK_FLETREF);

RULE(local_ref,
  CHILD(expr)
  OPTIONAL(id),
  TK_VARREF, TK_LETREF);


GROUP(type,
  type_infix, type_tuple, type_arrow, type_this, box_type, nominal,
  type_param_ref, dont_care);

RULE(type_infix, ONE_OR_MORE(type), TK_UNIONTYPE, TK_ISECTTYPE);

RULE(type_tuple, ONE_OR_MORE(type), TK_TUPLETYPE);

RULE(type_arrow,
  CHILD(type)
  CHILD(type),
  TK_ARROW);

RULE(nominal,
  HAS_DATA  // Definition of referred type
  CHILD(id, none) // Package
  CHILD(id)       // Type
  CHILD(type_args, none)
  CHILD(cap, gencap, none)
  CHILD(borrowed, ephemeral, none)
  OPTIONAL(id, none), // Original package specifier (for error reporting)
  TK_NOMINAL);

RULE(type_param_ref,
  HAS_DATA  // Definition of referred type parameter
  CHILD(id)
  CHILD(cap, gencap, none)
  CHILD(borrowed, ephemeral, none),
  TK_TYPEPARAMREF);

RULE(at, LEAF, TK_AT);
RULE(bool_literal, LEAF, TK_TRUE, TK_FALSE);
RULE(borrowed, LEAF, TK_BORROWED);
RULE(box_type, LEAF, TK_BOXTYPE);
RULE(cap, LEAF, TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
RULE(dont_care, LEAF, TK_DONTCARE);
RULE(ellipsis, LEAF, TK_ELLIPSIS);
RULE(ephemeral, LEAF, TK_EPHEMERAL);
RULE(float_literal, LEAF, TK_FLOAT);
RULE(gencap, LEAF, TK_CAP_READ, TK_CAP_SEND, TK_CAP_SHARE, TK_CAP_ANY);
RULE(id, LEAF, TK_ID);
RULE(int_literal, LEAF, TK_INT);
RULE(none, LEAF, TK_NONE);
RULE(question, LEAF, TK_QUESTION);
RULE(semi, LEAF, TK_SEMI);
RULE(string, LEAF, TK_STRING);
RULE(this_ref, LEAF, TK_THIS);
RULE(type_this, LEAF, TK_THISTYPE);
