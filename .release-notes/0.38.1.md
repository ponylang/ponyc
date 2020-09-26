## Fix race condition that can result in a segfault

We recently introduced some improvements to the handling of garbage collection for short-lived actors when the cycle detector is running. In the process, we introduced a race condition in the runtime that could result in an actor being garbage collected twice which if it were to occur, would result in a crashing program.

