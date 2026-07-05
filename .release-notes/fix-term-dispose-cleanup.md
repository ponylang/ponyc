## Fix `ANSITerm` not fully cleaning up on dispose

Disposing an `ANSITerm` did not fully shut it down: its `prompt` and `size` behaviors still called the notifier afterward, and the terminal-resize handler it installs was never removed, so it kept calling the closed notifier on a resize and held its signal subscription. A disposed `ANSITerm` now ignores `prompt` and `size` and removes its resize handler.
