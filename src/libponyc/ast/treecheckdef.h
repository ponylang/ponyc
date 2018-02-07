#ifndef TREE_CHECK
#error "This file should only be included by treecheck.c"
#endif

// TODO: big comment explaining what's going on

ROOT(program, module);

RULE(program,
  IS_SCOPE  // path -> PACKAGE
  HAS_DATA  // program_t
  ZERO_OR_MORE(package),
  TK_PROGRAM);

RULE(package,
  IS_SCOPE  // name -> entity
  HAS_DATA  // package_t
  ZERO_OR_MORE(module)
  OPTIONAL(string), // Package doc string
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
  HAS_TYPE(type)
  CHILD(id)
  CHILD(type, none)  // Field type
  CHILD(expr, none)
  CHILD(string, none),
  TK_FLET, TK_FVAR, TK_EMBED);

RULE(method,
  IS_SCOPE  // name -> TYPEPARAM | PARAM
  HAS_DATA  // Body donor type
  CHILD(cap, at, none)
  CHILD(id)
  CHILD(type_params, none)
  CHILD(params, none)
  CHILD(type, none) // Return type
  CHILD(question, none)
  CHILD(rawseq, none)  // Body
  CHILD(string, none)
  CHILD(rawseq, none), // Guard (case methods only)
  TK_FUN, TK_NEW, TK_BE);

RULE(type_params, ONE_OR_MORE(type_param), TK_TYPEPARAMS);

RULE(type_param,
  HAS_DATA // Original typeparam definition
  CHILD(id)
  CHILD(type, none)   // Constraint
  CHILD(type, none),  // Default
  TK_TYPEPARAM);

RULE(type_args, ONE_OR_MORE(type), TK_TYPEARGS);

RULE(params,
  ZERO_OR_MORE(param)
  OPTIONAL(ellipsis),
  TK_PARAMS);

RULE(param,
  HAS_TYPE(type)
  CHILD(id, expr)
  CHILD(type, none)
  CHILD(seq, none),
  TK_PARAM);

RULE(seq,
  IS_SCOPE  // name -> local ID node
  HAS_TYPE(type)
  ONE_OR_MORE(jump, intrinsic, compile_error, expr, semi),
  TK_SEQ);

RULE(rawseq,
  HAS_TYPE(type)
  ONE_OR_MORE(jump, intrinsic, compile_error, expr, semi),
  TK_SEQ);

RULE(seq_or_rawseq,
  MAYBE_SCOPE
  HAS_TYPE(type)
  ONE_OR_MORE(jump, intrinsic, compile_error, expr, semi),
  TK_SEQ);

RULE(jump,
  HAS_TYPE(type)
  CHILD(rawseq, this_ref, none),
  TK_RETURN, TK_BREAK, TK_CONTINUE, TK_ERROR);

RULE(intrinsic,
  HAS_TYPE(type)
  CHILD(none),
  TK_COMPILE_INTRINSIC);

RULE(compile_error,
  HAS_TYPE(type)
  CHILD(rawseq),
  TK_COMPILE_ERROR);

GROUP(expr,
  local, binop, isop, assignop, asop, tuple, consume, recover, prefix, dot,
  tilde, chain, qualify, call, ffi_call, match_capture,
  if_expr, ifdef, iftypeset, whileloop, repeat, for_loop, with, match, try_expr,
  lambda, barelambda, array_literal, object_literal, int_literal, float_literal,
  string, bool_literal, id, rawseq, package_ref, location,
  this_ref, ref, fun_ref, type_ref, field_ref, tuple_elem_ref, local_ref,
  param_ref);

RULE(local,
  HAS_TYPE(type)
  CHILD(id)
  CHILD(type, none),
  TK_LET, TK_VAR, TK_DONTCARE);

RULE(match_capture,
  HAS_TYPE(type)
  CHILD(id)
  CHILD(type),
  TK_MATCH_CAPTURE, TK_MATCH_DONTCARE);

RULE(binop,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(expr)
  CHILD(question, none),
  TK_PLUS, TK_MINUS, TK_MULTIPLY, TK_DIVIDE, TK_MOD, TK_LSHIFT, TK_RSHIFT,
  TK_PLUS_TILDE, TK_MINUS_TILDE, TK_MULTIPLY_TILDE, TK_DIVIDE_TILDE,
  TK_MOD_TILDE, TK_LSHIFT_TILDE, TK_RSHIFT_TILDE,
  TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
  TK_EQ_TILDE, TK_NE_TILDE, TK_LT_TILDE, TK_LE_TILDE, TK_GT_TILDE, TK_GE_TILDE,
  TK_AND, TK_OR, TK_XOR);

RULE(isop,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(expr),
  TK_IS, TK_ISNT);

RULE(assignop,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(expr),
  TK_ASSIGN);

RULE(asop,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(type),
  TK_AS);

RULE(tuple,
  HAS_TYPE(type)
  ONE_OR_MORE(rawseq),
  TK_TUPLE);

RULE(consume,
  HAS_TYPE(type)
  CHILD(cap, aliased, none)
  CHILD(expr),
  TK_CONSUME);

RULE(recover,
  HAS_TYPE(type)
  CHILD(cap, none)
  CHILD(seq),
  TK_RECOVER);

RULE(prefix,
  HAS_TYPE(type)
  CHILD(expr),
  TK_NOT, TK_UNARY_MINUS, TK_UNARY_MINUS_TILDE, TK_ADDRESS, TK_DIGESTOF);

RULE(dot,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(id, int_literal, type_args)
  IF((CHILD_ID(0) == TK_THIS), HAS_DATA),
  TK_DOT);

RULE(tilde,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(id),
  TK_TILDE);

RULE(chain,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(id),
  TK_CHAIN);

RULE(qualify,
  CHILD(expr)
  CHILD(type_args),
  TK_QUALIFY);

RULE(call,
  HAS_TYPE(type)
  CHILD(expr) // receiver
  CHILD(positional_args, none)
  CHILD(named_args, none)
  CHILD(question, none),
  TK_CALL);

RULE(ffi_call,
  HAS_TYPE(type)
  HAS_DATA  // FFI declaration to use.
  CHILD(id, string)
  CHILD(type_args, none)
  CHILD(positional_args, none)
  CHILD(named_args, none)
  CHILD(question, none),
  TK_FFICALL);

RULE(positional_args, ZERO_OR_MORE(seq_or_rawseq), TK_POSITIONALARGS);

RULE(named_args, ONE_OR_MORE(named_arg), TK_NAMEDARGS);

RULE(named_arg,
  CHILD(id)
  CHILD(rawseq),
  TK_NAMEDARG, TK_UPDATEARG);

RULE(ifdef,
  IS_SCOPE
  HAS_TYPE(type)
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

RULE(iftypeset,
  IS_SCOPE
  HAS_TYPE(type)
  CHILD(iftype) // Condition/then body
  CHILD(seq, iftypeset, none), // Else body
  TK_IFTYPE_SET);

RULE(iftype,
  IS_SCOPE
  HAS_TYPE(type)
  CHILD(type) // Subtype
  CHILD(type) // Supertype
  CHILD(seq) // Then body
  CHILD(type_params, none),
  TK_IFTYPE);

RULE(if_expr,
  IS_SCOPE
  HAS_TYPE(type)
  CHILD(rawseq)  // Condition
  CHILD(seq)  // Then body
  CHILD(seq, if_expr, none), // Else body
  TK_IF);

RULE(whileloop,
  IS_SCOPE
  HAS_TYPE(type)
  CHILD(rawseq)     // Condition
  CHILD(seq)        // Loop body
  CHILD(seq, none), // Else body
  TK_WHILE);

RULE(repeat,
  IS_SCOPE
  HAS_TYPE(type)
  CHILD(seq)        // Loop body
  CHILD(rawseq)        // Condition
  CHILD(seq, none), // Else body
  TK_REPEAT);

RULE(for_loop,
  HAS_TYPE(type)
  CHILD(expr)    // Iterator declaration
  CHILD(rawseq)  // Iterator value
  CHILD(rawseq)  // Loop body
  CHILD(seq, none), // Else body
  TK_FOR);

RULE(with,
  HAS_TYPE(type)
  CHILD(expr)    // With variable(s)
  CHILD(rawseq)  // Body
  CHILD(rawseq, none), // Else
  TK_WITH);

RULE(match,
  IS_SCOPE
  HAS_TYPE(type)
  CHILD(rawseq)
  CHILD(cases, none)
  CHILD(seq, none),  // Else body
  TK_MATCH);

RULE(cases,
  IS_SCOPE  // Cases is a scope to simplify branch consolidation.
  HAS_TYPE(type)  // Union of case types or "TK_CASES"
  ZERO_OR_MORE(match_case),
  TK_CASES);

RULE(match_case,
  IS_SCOPE
  HAS_TYPE(type)
  CHILD(expr, no_case_expr)
  CHILD(rawseq, none)   // Guard
  CHILD(rawseq, none),  // Body
  TK_CASE);

RULE(no_case_expr, HAS_TYPE(dontcare_type), TK_NONE);

RULE(try_expr,
  HAS_TYPE(type)
  CHILD(seq)  // Try body
  CHILD(seq, none)  // Else body
  CHILD(seq, none), // Then body. LLVMValueRef for the indirectbr instruction.
  TK_TRY, TK_TRY_NO_CHECK);

RULE(lambda,
  HAS_TYPE(type)
  CHILD(cap, none) // Receiver cap
  CHILD(id, none)
  CHILD(type_params, none)
  CHILD(params, none)
  CHILD(lambda_captures, none)
  CHILD(type, none) // Return
  CHILD(question, none)
  CHILD(rawseq)
  CHILD(cap, none), // Type reference cap
  TK_LAMBDA);

RULE(lambda_captures, ONE_OR_MORE(lambda_capture), TK_LAMBDACAPTURES);

RULE(lambda_capture,
  CHILD(id)
  CHILD(type, none)
  CHILD(expr, none),
  TK_LAMBDACAPTURE);

RULE(barelambda,
  HAS_TYPE(type)
  CHILD(at, none) // Receiver cap
  CHILD(id, none)
  CHILD(none) // Typeparams
  CHILD(params, none)
  CHILD(none) // Captures
  CHILD(type, none) // Return
  CHILD(question, none)
  CHILD(rawseq)
  CHILD(cap_val, none), // Type reference cap
  TK_BARELAMBDA);

RULE(array_literal,
  CHILD(type, none)
  CHILD(rawseq, none),
  TK_ARRAY);

RULE(object_literal,
  HAS_DATA  // Nice name to use for anonymous type, optional.
  CHILD(cap, none)
  CHILD(provides, none)
  CHILD(members),
  TK_OBJECT);

RULE(ref,
  HAS_TYPE(type)
  CHILD(id),
  TK_REFERENCE);

RULE(package_ref,
  HAS_DATA // Package import
  CHILD(id),
  TK_PACKAGEREF);

RULE(fun_ref,
  HAS_TYPE(type)
  HAS_DATA // Method definition
  CHILD(expr)
  CHILD(id, type_args),
  TK_FUNREF, TK_BEREF, TK_NEWREF, TK_NEWBEREF, TK_FUNCHAIN, TK_BECHAIN);

RULE(type_ref,
  HAS_TYPE(type)
  HAS_DATA // Type definition
  CHILD(package_ref, none)
  CHILD(id, none)
  CHILD(type_args, none),
  TK_TYPEREF);

RULE(field_ref,
  HAS_TYPE(type)
  HAS_DATA // Field definition
  CHILD(expr)
  CHILD(id),
  TK_FVARREF, TK_FLETREF, TK_EMBEDREF);

RULE(tuple_elem_ref,
  HAS_TYPE(type)
  CHILD(expr)
  CHILD(int_literal),
  TK_TUPLEELEMREF);

RULE(local_ref,
  HAS_TYPE(type)
  HAS_DATA // Local definition
  CHILD(expr)
  OPTIONAL(id),
  TK_VARREF, TK_LETREF, TK_DONTCAREREF);

RULE(param_ref,
  HAS_TYPE(type)
  HAS_DATA // Parameter definition
  CHILD(id),
  TK_PARAMREF);


GROUP(type,
  type_infix, type_tuple, type_arrow, type_this, cap, nominal,
  type_param_ref, dontcare_type, fun_type, error_type, infer_type, lambda_type,
  barelambda_type, literal_type, opliteral_type, control_type);

RULE(type_infix, ONE_OR_MORE(type), TK_UNIONTYPE, TK_ISECTTYPE);

RULE(type_tuple, ONE_OR_MORE(type), TK_TUPLETYPE);

RULE(type_arrow,
  CHILD(type)
  CHILD(type),
  TK_ARROW);

RULE(fun_type,
  CHILD(cap, at)
  CHILD(type_params, none)
  CHILD(params, none)
  CHILD(type, none), // Return type
  TK_FUNTYPE);

RULE(lambda_type,
  CHILD(cap, none)  // Apply function cap
  CHILD(id, none)
  CHILD(type_params, none)
  CHILD(type_list, none)  // Params
  CHILD(type, none) // Return type
  CHILD(question, none)
  CHILD(cap, gencap, none)  // Type reference cap
  CHILD(aliased, ephemeral, none),
  TK_LAMBDATYPE);

RULE(barelambda_type,
  CHILD(none) // Apply function cap
  CHILD(id, none)
  CHILD(none) // Typeparams
  CHILD(type_list, none) // Params
  CHILD(type, none) // Return type
  CHILD(question, none)
  CHILD(cap_val, none) // Type reference cap
  CHILD(aliased, ephemeral, none),
  TK_BARELAMBDATYPE);

RULE(type_list, ONE_OR_MORE(type), TK_PARAMS);

RULE(nominal,
  HAS_DATA  // Definition of referred type
  CHILD(id, none) // Package
  CHILD(id)       // Type
  CHILD(type_args, none)
  CHILD(cap, gencap, none)
  CHILD(aliased, ephemeral, none)
  OPTIONAL(id, none), // Original package specifier (for error reporting)
  TK_NOMINAL);

RULE(type_param_ref,
  HAS_DATA  // Definition of referred type parameter
  CHILD(id)
  CHILD(cap, gencap, none)
  CHILD(aliased, ephemeral, none),
  TK_TYPEPARAMREF);

RULE(at, LEAF, TK_AT);
RULE(bool_literal, HAS_TYPE(type), TK_TRUE, TK_FALSE);
RULE(aliased, LEAF, TK_ALIASED);
RULE(cap, LEAF, TK_ISO, TK_TRN, TK_REF, TK_VAL, TK_BOX, TK_TAG);
RULE(cap_val, LEAF, TK_VAL);
RULE(control_type, LEAF, TK_IF, TK_CASES, TK_COMPILE_INTRINSIC,
  TK_COMPILE_ERROR, TK_RETURN, TK_BREAK, TK_CONTINUE, TK_ERROR);
RULE(dontcare_type, LEAF, TK_DONTCARETYPE);
RULE(ellipsis, LEAF, TK_ELLIPSIS);
RULE(ephemeral, LEAF, TK_EPHEMERAL);
RULE(error_type, LEAF, TK_ERRORTYPE);
RULE(infer_type, LEAF, TK_INFERTYPE);
RULE(float_literal, HAS_TYPE(type), TK_FLOAT);
RULE(gencap, LEAF, TK_CAP_READ, TK_CAP_SEND, TK_CAP_SHARE, TK_CAP_ALIAS,
  TK_CAP_ANY);
RULE(id, HAS_TYPE(type) HAS_DATA, TK_ID);
RULE(int_literal, HAS_TYPE(type), TK_INT);
RULE(literal_type, LEAF, TK_LITERAL, TK_LITERALBRANCH);
RULE(location, HAS_TYPE(nominal), TK_LOCATION);
RULE(none, LEAF, TK_NONE);
RULE(opliteral_type, HAS_DATA, TK_OPERATORLITERAL);
RULE(question, LEAF, TK_QUESTION);
RULE(semi, LEAF, TK_SEMI);
RULE(string, HAS_TYPE(type), TK_STRING);
RULE(this_ref, HAS_TYPE(type), TK_THIS);
RULE(type_this, LEAF, TK_THISTYPE);
