## Fix `Readline.down` underflow on empty history

Pressing down in a `Readline` with no history triggered an integer underflow and an out-of-bounds history access. Both are now avoided.
