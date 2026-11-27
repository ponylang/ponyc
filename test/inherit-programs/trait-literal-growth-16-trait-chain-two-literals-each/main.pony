// Sixteen traits each return two object literals that capture this and are the
// next trait. The number of classes must grow with the number of literals,
// not double per level. Expects bat0.
trait T0
  fun name(): String => "t0"
  fun val fa(): T1 val =>
    let me = this
    object val is T1
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T1 val =>
    let me = this
    object val is T1
      fun name(): String => "b" + me.name()
    end
trait T1
  fun name(): String => "t1"
  fun val fa(): T2 val =>
    let me = this
    object val is T2
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T2 val =>
    let me = this
    object val is T2
      fun name(): String => "b" + me.name()
    end
trait T2
  fun name(): String => "t2"
  fun val fa(): T3 val =>
    let me = this
    object val is T3
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T3 val =>
    let me = this
    object val is T3
      fun name(): String => "b" + me.name()
    end
trait T3
  fun name(): String => "t3"
  fun val fa(): T4 val =>
    let me = this
    object val is T4
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T4 val =>
    let me = this
    object val is T4
      fun name(): String => "b" + me.name()
    end
trait T4
  fun name(): String => "t4"
  fun val fa(): T5 val =>
    let me = this
    object val is T5
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T5 val =>
    let me = this
    object val is T5
      fun name(): String => "b" + me.name()
    end
trait T5
  fun name(): String => "t5"
  fun val fa(): T6 val =>
    let me = this
    object val is T6
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T6 val =>
    let me = this
    object val is T6
      fun name(): String => "b" + me.name()
    end
trait T6
  fun name(): String => "t6"
  fun val fa(): T7 val =>
    let me = this
    object val is T7
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T7 val =>
    let me = this
    object val is T7
      fun name(): String => "b" + me.name()
    end
trait T7
  fun name(): String => "t7"
  fun val fa(): T8 val =>
    let me = this
    object val is T8
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T8 val =>
    let me = this
    object val is T8
      fun name(): String => "b" + me.name()
    end
trait T8
  fun name(): String => "t8"
  fun val fa(): T9 val =>
    let me = this
    object val is T9
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T9 val =>
    let me = this
    object val is T9
      fun name(): String => "b" + me.name()
    end
trait T9
  fun name(): String => "t9"
  fun val fa(): T10 val =>
    let me = this
    object val is T10
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T10 val =>
    let me = this
    object val is T10
      fun name(): String => "b" + me.name()
    end
trait T10
  fun name(): String => "t10"
  fun val fa(): T11 val =>
    let me = this
    object val is T11
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T11 val =>
    let me = this
    object val is T11
      fun name(): String => "b" + me.name()
    end
trait T11
  fun name(): String => "t11"
  fun val fa(): T12 val =>
    let me = this
    object val is T12
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T12 val =>
    let me = this
    object val is T12
      fun name(): String => "b" + me.name()
    end
trait T12
  fun name(): String => "t12"
  fun val fa(): T13 val =>
    let me = this
    object val is T13
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T13 val =>
    let me = this
    object val is T13
      fun name(): String => "b" + me.name()
    end
trait T13
  fun name(): String => "t13"
  fun val fa(): T14 val =>
    let me = this
    object val is T14
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T14 val =>
    let me = this
    object val is T14
      fun name(): String => "b" + me.name()
    end
trait T14
  fun name(): String => "t14"
  fun val fa(): T15 val =>
    let me = this
    object val is T15
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T15 val =>
    let me = this
    object val is T15
      fun name(): String => "b" + me.name()
    end
trait T15
  fun name(): String => "t15"
  fun val fa(): T16 val =>
    let me = this
    object val is T16
      fun name(): String => "a" + me.name()
    end
  fun val fb(): T16 val =>
    let me = this
    object val is T16
      fun name(): String => "b" + me.name()
    end
trait T16
  fun name(): String => "t16"
class val C is T0
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.fa().fb().name())
