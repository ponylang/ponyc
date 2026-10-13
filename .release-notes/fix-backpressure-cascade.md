## Fix backpressure muting cascade

Under fan-in topologies — many actors sending to a shared slow receiver — the runtime's binary muting cascaded backward through the entire actor graph. When the receiver became overloaded, every actor that sent to it was immediately muted. Their senders then saw muted targets and were muted in turn. Within one scheduling interval, most of the system was frozen, dropping throughput from tens of millions of messages per second to near zero.

The runtime now uses graduated per-actor batch sizes instead of binary muting. Each actor's batch size adjusts dynamically: senders to overloaded receivers are throttled proportionally (one step per send) rather than muted outright, and overloaded receivers process larger batches to catch up. Actors are only fully muted after repeated sends to overloaded receivers within a single scheduling run. This eliminates the cascade — throttled actors remain scheduled, so their senders never see them as muted.
