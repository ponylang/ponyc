use @pony_exitcode[None](code: I32) 

class Foo
    embed _array: Array[I32] = []

new create() => None
new clone(src: Foo) => src._array.copy_to(_array, 0, 0, 10)

actor Main

    new create(env': Env) =>
        let f1 = Foo
        let f2 = Foo.clone(f1)
        @pony_exitcode(1)
