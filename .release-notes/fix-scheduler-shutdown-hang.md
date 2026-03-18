## Fix intermittent hang during runtime shutdown

There was a race condition in the scheduler shutdown sequence that could cause the runtime to hang indefinitely instead of exiting. When the runtime initiated shutdown, it woke all suspended scheduler threads and sent them a terminate message. But a thread could re-suspend before processing the terminate, and once other threads exited, the suspend loop's condition (`active_scheduler_count <= thread_index`) became permanently true — trapping the thread in an endless sleep cycle with nobody left to wake it.

This was more likely to trigger on slower systems (VMs, heavily loaded machines) where the timing window was wider, and only affected programs using multiple scheduler threads. The fix adds a shutdown flag that the suspend loop checks, preventing threads from re-entering sleep once shutdown has begun.
