// C inherits T.mk from t.pony, whose object literal captures a local. The
// literal in Main must report its type name through __loc as $1$4.
class C is T
actor Main
  new create(env: Env) =>
    C.mk()
    let o = object val fun n(): String => __loc.type_name() end
    env.out.print(o.n())
