"""
Unlike case 1 (test/full-program-tests/issue-4475-case-1/main.pony) of the
#4475 issue test, this code passed before the fix because the Pony
statement error was not generated before a termination instruction (e.g. ret).
"""
class Foo
  new create(a: U32) ? =>
    if a > 10 then
      error
    end

actor Main
  new create(env: Env) =>
    try
      let f = Foo(1)?
    end
