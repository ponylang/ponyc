## Fix garbage collection when a tag aliases a val or iso reference

A Pony program could crash because the garbage collector freed data an actor could still reach. This could happen when the actor held a `tag` reference to an object and could also read that object's fields through another reference. The two references could be in the actor's own fields or in a message it sent or received, such as a message carrying both a `tag` and a `val` or `iso`. The runtime now keeps the data alive while the actor can still reach it.
