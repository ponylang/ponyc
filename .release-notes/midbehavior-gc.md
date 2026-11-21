## Allow garbage collection during a behavior

An actor can now reclaim memory during a long-running behavior. If the behavior repeatedly creates temporary objects, the actor can reuse some of their storage before the behavior returns. After a call to `pony_triggergc`, the runtime can collect before the current behavior returns. User finalizers still run between behaviors.

If the runtime cannot trace all live references in a method or constructor, collection during the behavior may have to wait until that call returns. The runtime also skips collection points inside a behavior while another actor holds a reference to one of this actor's objects or this actor holds a reference to a managed object owned by another actor.

An actor that calls a foreign function that may retain a Pony pointer, including a function the compiler has not classified, collects only between behaviors for the rest of its lifetime. Using a raw pointer can have the same effect unless the compiler recognizes all its uses, such as checking it for null. An actor can still collect after initializing a struct field directly with `Pointer.create()` or `NullablePointer.none()`. A callback from native code can also restrict an actor to collection between behaviors for its lifetime.
