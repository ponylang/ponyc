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
  HAS_TYPE
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
  HAS_TYPE
  CHILD(id, expr)
  CHILD(type, none)
  CHILD(expr, none),
  TK_PARAM);

RULE(seq,
  MAYBE_SCOPE
  HAS_TYPE
  ONE_OR_MORE(jump, intrinsic, compile_error, expr, semi),
  TK_SEQ);

RULE(jump,
  HAS_TYPE
  OPTIONAL(seq, none),
  TK_RETURN, TK_BREAK, TK_CONTINUE, TK_ERROR);

RULE(intrinsic,
  HAS_TYPE
  OPTIONAL(none),
  TK_COMPILE_INTRINSIC);

RULE(compile_error,
  HAS_TYPE
  CHILD(seq),
  TK_COMPILE_ERROR);

GROUP(expr,
  local, infix, asop, tuple, consume, recover, prefix, dot, tilde,
  qualify, call, ffi_call,
  if_expr, ifdef, loop, for_loop, with, match, try_expr, lambda,
  array_literal, object_literal, int_literal, float_literal, string,
  bool_literal, id, seq, package_ref,
  this_ref, ref, fun_ref, type_ref, flet_ref, field_ref, local_ref);

RULE(local,
  HAS_TYPE
  CHILD(id)
  CHILD(type, none),
  TK_LET, TK_VAR);

RULE(infix,
  HAS_TYPE
  // RHS first for TK_ASSIGN, to handle init tracking
  // LHS first for all others
  CHILD(expr)
  CHILD(expr),
  TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD, TK_LSHIFT, TK_RSHIFT,
  TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE, TK_IS, TK_ISNT,
  TK_AND, TK_OR, TK_XOR, TK_ASSIGN);

RULE(asop,
  HAS_TYPE
  CHILD(expr)
  CHILD(type),
  TK_AS);

RULE(tuple,
  HAS_TYPE
  ONE_OR_MORE(seq, dont_care),
  TK_TUPLE);

RULE(consume,
  HAS_TYPE
  CHILD(cap, borrowed, none)
  CHILD(expr),
  TK_CONSUME);

RULE(recover,
  HAS_TYPE
  IS_SCOPE
  CHILD(cap, none)
  CHILD(expr),
  TK_RECOVER);

RULE(prefix,
  HAS_TYPE
  CHILD(expr),
  TK_NOT, TK_UNARY_MINUS, TK_ADDRESS, TK_IDENTITY);

RULE(dot,
  HAS_TYPE
  CHILD(expr)
  CHILD(id, int_literal, type_args),
  TK_DOT);

RULE(tilde,
  HAS_TYPE
  CHILD(expr)
  CHILD(id),
  TK_TILDE);

RULE(qualify,
  CHILD(expr)
  CHILD(type_args),
  TK_QUALIFY);

RULE(call,
  HAS_TYPE
  CHILD(positional_args, none)
  CHILD(named_args, none)
  CHILD(expr),  // Note that receiver comes last
  TK_CALL);

RULE(ffi_call,
  HAS_TYPE
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
  HAS_TYPE
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
  MAYBE_SCOPE // Scope in an expression, not when used as a type
  HAS_TYPE
  // All children are present in an expression, none when used as a type
  OPTIONAL(seq)  // Condition (has no symbol table)
  OPTIONAL(seq)  // Then body
  OPTIONAL(seq, if_expr, none), // Else body
  TK_IF);

RULE(loop,
  IS_SCOPE
  HAS_TYPE
  CHILD(seq)        // Condition
  CHILD(seq)        // Loop body
  CHILD(seq, none), // Else body
  TK_WHILE, TK_REPEAT);
  // TODO: This is wrong for repeat, check parser.c is actually how we want it

RULE(for_loop,
  HAS_TYPE
  CHILD(expr) // Iterator declaration
  CHILD(seq)  // Iterator value
  CHILD(seq)  // Loop body
  CHILD(seq, none), // Else body
  TK_FOR);

RULE(with,
  HAS_TYPE
  CHILD(expr) // With variable(s)
  CHILD(seq)  // Body
  CHILD(seq, none), // Else
  TK_WITH);

RULE(match,
  IS_SCOPE
  HAS_TYPE
  CHILD(expr)
  CHILD(cases, none)
  CHILD(seq, none),  // Else body
  TK_MATCH);

RULE(cases,
  MAYBE_SCOPE // Has scope in a match, not when a type
  HAS_TYPE  // Union of case types or "TK_CASES"
  ZERO_OR_MORE(match_case),
  TK_CASES);

RULE(match_case,
  IS_SCOPE
  CHILD(expr, none)
  CHILD(seq, none)   // Guard
  CHILD(seq, none),  // Body
  TK_CASE);

RULE(try_expr,
  HAS_TYPE
  CHILD(seq)  // Try body
  CHILD(seq, none)  // Else body
  CHILD(seq, none), // Then body. LLVMValueRef for the indirectbr instruction.
  TK_TRY, TK_TRY_NO_CHECK);

RULE(lambda,
  HAS_TYPE
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
  HAS_TYPE
  CHILD(id),
  TK_REFERENCE, TK_PARAMREF);

RULE(package_ref,
  CHILD(id),
  TK_PACKAGEREF);

RULE(fun_ref,
  HAS_TYPE
  CHILD(expr)
  CHILD(id, type_args),
  TK_FUNREF, TK_BEREF, TK_NEWREF, TK_NEWBEREF);

RULE(type_ref,
  HAS_TYPE
  CHILD(expr)
  OPTIONAL(id, type_args),
  TK_TYPEREF);

RULE(field_ref,
  HAS_TYPE
  CHILD(expr)
  CHILD(id),
  TK_FVARREF, TK_EMBEDREF);

RULE(flet_ref,
  HAS_TYPE
  CHILD(expr)
  CHILD(id, int_literal), // Int for tuple element access
  TK_FLETREF);

RULE(local_ref,
  HAS_TYPE
  CHILD(expr)
  OPTIONAL(id),
  TK_VARREF, TK_LETREF);


GROUP(type,
  type_infix, type_tuple, type_arrow, type_this, box_type, nominal,
  type_param_ref, dont_care, fun_type, error_type,
  literal_type, opliteral_type, jump, intrinsic, compile_error,
  cases, if_expr, dont_care);

RULE(type_infix, ONE_OR_MORE(type), TK_UNIONTYPE, TK_ISECTTYPE);

RULE(type_tuple, ONE_OR_MORE(type), TK_TUPLETYPE);

RULE(type_arrow,
  CHILD(type)
  CHILD(type),
  TK_ARROW);

RULE(fun_type,
  CHILD(cap)
  CHILD(type_params, none)
  CHILD(params, none)
  CHILD(type, none), // Return type
  TK_FUNTYPE);

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
RULE(bool_literal, HAS_TYPE, TK_TRUE, TK_FALSE);
RULE(borrowed, LEAF, TK_BORROWED);
RULE(box_type, LEAF, TK_BOXTYPE);
RULE(cap, LEAF, TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
RULE(dont_care, HAS_TYPE, TK_DONTCARE);
RULE(ellipsis, LEAF, TK_ELLIPSIS);
RULE(ephemeral, LEAF, TK_EPHEMERAL);
RULE(error_type, LEAF, TK_ERRORTYPE);
RULE(float_literal, HAS_TYPE, TK_FLOAT);
RULE(gencap, LEAF, TK_CAP_READ, TK_CAP_SEND, TK_CAP_SHARE, TK_CAP_ANY);
RULE(id, HAS_TYPE, TK_ID);
RULE(int_literal, HAS_TYPE, TK_INT);
RULE(literal_type, LEAF, TK_LITERAL, TK_LITERALBRANCH);
RULE(none, LEAF, TK_NONE);
RULE(opliteral_type, HAS_DATA, TK_OPERATORLITERAL);
RULE(question, LEAF, TK_QUESTION);
RULE(semi, LEAF, TK_SEMI);
RULE(string, HAS_TYPE, TK_STRING);
RULE(this_ref, HAS_TYPE, TK_THIS);
RULE(type_this, LEAF, TK_THISTYPE);
