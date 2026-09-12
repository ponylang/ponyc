"""
A microbenchmark for testing thundering herd/fan-in type workloads and how
backpressure impacts them in the Pony runtime. Based on `message-ubench` and
the description in issue #2980 to reproduce the thundering herd/fan-in behavior
in issue #2980.

The topology of this microbenchmark is the following:

  N `Sender` actors => M `Analyzer` actors => 1 `Receiver` actor

The logic is as follows:

* The `Sender` actors send messages as fast as they can to the `Analyzer`
  actors. The number of `Sender` actors is controlled by the `--senders` cli
  argument.
* The `Analyzer` actors receive messages from `Sender` actors and increment a
  count. They only send messages to the `Receiver` actor when a tick fires. The
  number of `Analyzer` actors is controlled by the `--analyzers` cli argument.
* The `Receiver` actor receives messages from the `Analyzer` actors and does
  some "work" (simulated by `usleep`). The amount of "work" is controlled by
  the `--receiver-workload` cli argument.
* The `Coordinator` actor manages when ticks get fired using a timer and when a
  tick is fired it asks all `Analyzer` actors for a status. If an `Analyzer`
  actor is muted due to sending to the `Receiver` actor, it will not respond
  promptly and the reports printed by the `Coordinator` actor will go up and
  down as backpressure kicks in and out when the `Receiver` actor falls behind
  and catches up.
"""
