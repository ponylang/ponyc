## Fix scheduler threads suspending after half the configured idle time

The `--ponysuspendthreshold` runtime option controls how long a scheduler thread waits, while it has no work to do, before suspending itself to reduce CPU usage. It is documented in milliseconds and defaults to 1 ms.

On the most common platforms, including all x86 systems, the threshold was being applied at half its intended scale, so threads started suspending after about half the idle time you asked for. Passing `--ponysuspendthreshold 100` caused threads to begin suspending after roughly 50 ms instead of 100 ms, and the 1 ms default behaved like 0.5 ms.

The threshold now uses the same timing scale as the runtime's other interval options, such as `--ponycdinterval`, rather than half of it.

As a result, idle scheduler threads now stay awake roughly twice as long before suspending compared to previous releases. Programs that depend on scheduler threads suspending quickly when idle (for example, to keep CPU usage low on an otherwise idle process) may see threads remain active a little longer, while programs sensitive to the cost of waking a suspended thread when new work arrives may benefit. If you previously tuned `--ponysuspendthreshold`, halve your value to keep the same timing as before.

The same calibration error affected an internal threshold the runtime uses to decide a scheduler thread has gone idle, which doubled along with the suspend threshold. As a side effect, a program that goes idle may take a fraction of a millisecond longer to be detected as ready to terminate than in previous releases.
