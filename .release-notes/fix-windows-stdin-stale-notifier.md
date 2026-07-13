## Fix a new Windows stdin notifier getting a read posted for the one it replaced

On Windows, a program that disposed its stdin notifier and installed a new one could have the new notifier sent a read that was posted for the notifier it replaced.

Reading stdin through `env.input` and the standard library's `Stdin` actor, nothing was lost, duplicated, or reordered, so most programs saw no difference. A program that subscribed to stdin readiness through the asio event API directly could be sent a read posted for a notifier it had already replaced. A new notifier is now sent only the reads posted for it.
