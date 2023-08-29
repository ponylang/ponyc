use "lib:ponyc-standalone" if not openbsd or dragonfly
use "lib:c++" if osx

use @token_new[NullablePointer[TokenStub]](token_id: TokenId) if not openbsd or dragonfly
use @ast_new[NullablePointer[AstStub]](token: TokenStub, token_id: TokenId) if not openbsd or dragonfly
use @token_free[None](token: TokenStub) if not openbsd or dragonfly
use @ast_free[None](ast: AstStub) if not openbsd or dragonfly
use @ast_id[TokenId](ast: AstStub) if not openbsd or dragonfly

struct AstStub
struct TokenStub

type TokenId is I32

actor Main
  new create(env: Env) =>
    try
      ifdef not openbsd or dragonfly then
        let token = @token_new(2)()?
        let ast = @ast_new(token, 2)()?
        if @ast_id(ast) != 2 then
          env.exitcode(1)
        end
        @ast_free(ast)
        @token_free(token)
      else
        env.out.print("We are on an unsupported platform")
      end
    else
      env.exitcode(1)
    end
