// Fixture for `document_symbol/integration/lambda_members`.
//
// `_DsLambda` exercises lambda-related members:
// - `_callback`: a `let` field whose type is a lambda type
// - `ds_no_lambda`: a plain method with no lambda involvement
// - `ds_with_lambda`: a method whose body creates a lambda literal
// - `ds_returns_lambda`: a method whose return type is a lambda type
// - `ds_lambda_arg`: a method that accepts a lambda-typed parameter
// - `ds_local_lambda_type`: a method with a lambda-typed local variable
//
// None of the lambda type expressions or literals must appear as child
// symbols — `_DsLambda`'s outline must contain exactly the six named
// members above with their correct SymbolKinds.

class _DsLambda
  let _callback: {(U32): U32} val = {(x: U32): U32 => x }

  fun ds_no_lambda(): U32 => 0

  fun ds_with_lambda(): U32 =>
    let f = {(x: U32): U32 => x + 1 }
    f(0)

  fun ds_returns_lambda(): {(U32): U32} val =>
    {(x: U32): U32 => x + 1 }

  fun ds_lambda_arg(f: {(U32): U32} box): U32 =>
    f(0)

  fun ds_local_lambda_type(): U32 =>
    let f: {(U32): U32} val = {(x: U32): U32 => x }
    f(0)
