use "lib:ponyc-standalone" if freebsd or linux or osx or windows
use "lib:c++" if osx

use @token_new[NullablePointer[TokenStub]](token_id: TokenId) if freebsd or linux or osx or windows
use @ast_new[NullablePointer[AstStub]](token: TokenStub, token_id: TokenId) if freebsd or linux or osx or windows
use @token_free[None](token: TokenStub) if freebsd or linux or osx or windows
use @ast_free[None](ast: AstStub) if freebsd or linux or osx or windows
use @ast_id[TokenId](ast: AstStub) if freebsd or linux or osx or windows

struct AstStub
struct TokenStub

type TokenId is I32

actor Main
  new create(env: Env) =>
    try
      ifdef freebsd or linux or osx or windows then
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
