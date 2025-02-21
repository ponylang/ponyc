## Fix use-after-free triggered by fast actor reaping when the cycle detector is active

The logic in the actor run and the cycle detector work together to enable fast reaping of actors with rc == 0. This relies on atomics and protecting the relevant areas of logic with critical sections. This logic unfortunately suffered from a use-after-free bug due to a race between the cycle detector receiving the block message and destroying the actor and the actor cycle detector critical flag being release as identified in #4614 which could sometimes lead to memory corruption.

This commit changes things to remove the need to protect the logic with critical sections. It achieves this by ensuring that an actor with rc == 0 that the cycle detector knows about will never be rescheduled again even if the cycle detector happens to send it a message and the cycle detector is free to reap the actor when it receives the block message. The cycle detector ensures that the actor's message queue is empty or that the only messages pending are the expected ones from the cycle detector so it can safely destroy the actor.

## Fix incorrect printing of runtime stats

When the runtime was compiled with the runtime stats option on, some stats were being printed to standard out without them having been requested. We've fixed the issue so stats will only be printed if the option is turned on by the user.

## Fix memory leak

In the process of fixing a "in theory could be use after free" that tooling found, we created a real memory leak. We've reverted back to the "in theory bad, in practice not" code from before and fixed the leak.

## Fix being unable to compile programs that use runtime info on Windows

We accidentally weren't properly exporting the Pony runtime methods needed by the "runtime_info" package so that they could be linked on Windows.

