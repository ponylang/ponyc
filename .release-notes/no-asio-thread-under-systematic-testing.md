## Don't run the ASIO thread under systematic testing

Systematic testing (`use=systematic_testing`) replays a single scheduler interleaving from a seed so that a run is deterministic and reproducible. The ASIO thread — which handles sockets, standard input, process I/O, signals, and timers — is a real operating-system thread doing external work that a seed cannot control, so its presence left those runs only partly deterministic. A program that used any of that I/O under systematic testing would run anyway, carrying a source of nondeterminism the seed could not pin down.

Systematic testing no longer runs the ASIO thread. Any attempt to register an I/O event — opening a socket, reading standard input, spawning a process, installing a signal handler, or arming a timer — now aborts with a message saying I/O is not available under systematic testing. This is deliberate: rather than silently dropping the I/O and letting a program appear to be tested deterministically when it is not, the runtime says so plainly.

This removes the ASIO thread as a source of nondeterminism. It does not make every program deterministic on its own — a program can still introduce nondeterminism by other means, such as reading the wall clock through the FFI — and avoiding those remains the programmer's responsibility when a fully reproducible run is the goal.
