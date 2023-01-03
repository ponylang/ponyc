## Fix compiler segfault caused by infinite recursion

The following program used to cause ponyc to segfault due to infinite recursion:

```pony
type Message is ((I64 | String), String, None)

interface tag Manager
  be handle_message(msg: Message val)

actor Main
  new create(env: Env) =>
    Foo(env, recover tag this end)

  be handle_message(msg: Message val) =>
    None

actor Foo
  var manager: Manager

  new create(env: Env, manager': Manager) =>
    manager = manager'
    let notifier = InputNotifier(this)
    env.input(consume notifier)

  be handle_data(data: String) =>
    manager.handle_message(("","",None))

class InputNotifier is InputNotify
  let parent: Foo

  new iso create(parent': Foo) =>
    parent = parent'

  fun ref apply(data': Array[U8 val] iso) =>
    parent.handle_data("")
```
