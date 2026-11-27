// lib's Foo default partially applies Helper~create(), mid's Mid provides Foo,
// and Bar in main provides Mid. The test expects "lib.Helper": Helper resolves
// in lib when the body is inherited through a middle package.
use m = "mid"
class Bar is m.Mid
actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())
