use "lib:ponyc-standalone" if linux or osx or windows
use "lib:c++" if osx

use @token_new[NullablePointer[TokenStub]](token_id: TokenId)
use @ast_new[NullablePointer[AstStub]](token: TokenStub, token_id: TokenId)
use @token_free[None](token: TokenStub)
use @ast_free[None](ast: AstStub)
use @ast_id[TokenId](ast: AstStub)

struct AstStub
struct TokenStub

type TokenId is I32

actor Main
  new create(env: Env) =>
    try
      let token = @token_new(2)()?
      let ast = @ast_new(token, 2)()?
      if @ast_id(ast) != 2 then
        env.exitcode(1)
      en
      @ast_free(ast)
      @token_free(token)
    else
      env.exitcode(1)
    end
